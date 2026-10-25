use std::ffi::c_char;
use std::ptr;

#[no_mangle]
pub unsafe extern "C" fn reqflow_expand_template(
    input: *const c_char,
    input_len: usize,
    variables_json: *const c_char,
    variables_json_len: usize,
    output_len_out: *mut usize,
    error_out: *mut *mut c_char,
    error_len_out: *mut usize,
) -> *mut c_char {
    if !output_len_out.is_null() {
        *output_len_out = 0;
    }
    if !error_out.is_null() {
        *error_out = ptr::null_mut();
    }
    if !error_len_out.is_null() {
        *error_len_out = 0;
    }

    let result = (|| -> Result<String, String> {
        if input.is_null() || variables_json.is_null() {
            return Err("Entrada nula al backend Rust.".to_owned());
        }
        let input_bytes = std::slice::from_raw_parts(input.cast::<u8>(), input_len);
        let json_bytes = std::slice::from_raw_parts(variables_json.cast::<u8>(), variables_json_len);
        let input = std::str::from_utf8(input_bytes)
            .map_err(|_| "La plantilla no es UTF-8 válido.".to_owned())?;
        let json_text = std::str::from_utf8(json_bytes)
            .map_err(|_| "El entorno no es UTF-8 válido.".to_owned())?;
        let variables: serde_json::Value = serde_json::from_str(json_text)
            .map_err(|_| "El entorno debe ser un objeto JSON válido.".to_owned())?;
        let object = variables
            .as_object()
            .ok_or_else(|| "El entorno debe ser un objeto JSON.".to_owned())?;
        expand_template(input, object)
    })();

    match result {
        Ok(value) => {
            let (output, output_len) = string_into_raw(value);
            if !output_len_out.is_null() {
                *output_len_out = output_len;
            }
            output
        },
        Err(message) => {
            if !error_out.is_null() && !error_len_out.is_null() {
                let (error, error_len) = string_into_raw(message);
                *error_out = error;
                *error_len_out = error_len;
            }
            ptr::null_mut()
        }
    }
}

fn string_into_raw(value: String) -> (*mut c_char, usize) {
    let boxed = value.into_bytes().into_boxed_slice();
    let len = boxed.len();
    (Box::into_raw(boxed) as *mut u8 as *mut c_char, len)
}

#[no_mangle]
pub unsafe extern "C" fn reqflow_string_free(value: *mut c_char, len: usize) {
    if !value.is_null() {
        let slice = ptr::slice_from_raw_parts_mut(value.cast::<u8>(), len);
        drop(Box::from_raw(slice));
    }
}

fn expand_template(
    input: &str,
    variables: &serde_json::Map<String, serde_json::Value>,
) -> Result<String, String> {
    let mut output = String::with_capacity(input.len());
    let mut cursor = 0;
    while let Some(relative_start) = input[cursor..].find("{{") {
        let start = cursor + relative_start;
        output.push_str(&input[cursor..start]);
        let token_start = start + 2;
        let relative_end = input[token_start..]
            .find("}}")
            .ok_or_else(malformed)?;
        let end = token_start + relative_end;
        let name = &input[token_start..end];
        if !valid_identifier(name) {
            return Err(malformed());
        }
        let value = variables
            .get(name)
            .ok_or_else(|| format!("La variable {{{{{name}}}}} no existe en el entorno seleccionado."))?;
        output.push_str(&json_value_to_template_string(value));
        cursor = end + 2;
    }
    output.push_str(&input[cursor..]);
    if output.contains("{{") || output.contains("}}") {
        return Err(malformed());
    }
    Ok(output)
}

fn malformed() -> String {
    "Marcador de plantilla mal formado o anidado; se esperaba {{identificador}}.".to_owned()
}

fn valid_identifier(name: &str) -> bool {
    let mut chars = name.chars();
    let Some(first) = chars.next() else {
        return false;
    };
    (first == '_' || first.is_ascii_alphabetic())
        && chars.all(|ch| ch == '_' || ch == '-' || ch == '.' || ch.is_ascii_alphanumeric())
}

fn json_value_to_template_string(value: &serde_json::Value) -> String {
    match value {
        serde_json::Value::String(text) => text.clone(),
        serde_json::Value::Null => String::new(),
        serde_json::Value::Bool(value) => value.to_string(),
        serde_json::Value::Number(value) => value.to_string(),
        serde_json::Value::Array(_) | serde_json::Value::Object(_) => value.to_string(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn expands_multiple_tokens_without_mutating_input() {
        let vars = json!({"host":"https://api.example.test", "version":2});
        let input = "{{host}}/v{{version}}/{{host}}";
        assert_eq!(
            expand_template(input, vars.as_object().unwrap()).unwrap(),
            "https://api.example.test/v2/https://api.example.test"
        );
        assert_eq!(input, "{{host}}/v{{version}}/{{host}}");
    }

    #[test]
    fn rejects_missing_and_malformed_tokens() {
        let vars = json!({"ok":"yes"});
        let obj = vars.as_object().unwrap();
        assert!(expand_template("{{missing}}", obj).unwrap_err().contains("missing"));
        assert!(expand_template("{{bad name}}", obj).is_err());
        assert!(expand_template("{{ok", obj).is_err());
        assert!(expand_template("prefix }}", obj).is_err());
    }

    #[test]
    fn formats_json_values_compactly_and_preserves_unicode() {
        let vars = json!({"object":{"k":1}, "array":[1,"a"], "null":null,
                          "bool":true, "number":2.5, "text":"Pánuco"});
        let obj = vars.as_object().unwrap();
        assert_eq!(
            expand_template("{{object}}|{{array}}|{{null}}|{{bool}}|{{number}}|{{text}}", obj).unwrap(),
            "{\"k\":1}|[1,\"a\"]||true|2.5|Pánuco"
        );
    }

    #[test]
    fn preserves_ordinary_braces_but_rejects_malformed_and_nested_markers() {
        let vars = json!({"name":"ReqFlow"});
        let obj = vars.as_object().unwrap();
        assert_eq!(expand_template("{literal} {{name}}", obj).unwrap(), "{literal} ReqFlow");
        assert!(expand_template("{{name}}}}", obj).is_err());
        assert!(expand_template("{{outer{{name}}}}", obj).is_err());
        assert!(expand_template("{{bad name}}", obj).is_err());
    }

    #[test]
    fn ffi_supports_embedded_nul_and_frees_allocator_matched_buffers() {
        let input = b"{{nul}}";
        let variables = br#"{"nul":"\u0000"}"#;
        let mut output_len = 0;
        let mut error = ptr::null_mut();
        let mut error_len = 0;
        let output = unsafe {
            reqflow_expand_template(
                input.as_ptr().cast(), input.len(),
                variables.as_ptr().cast(), variables.len(), &mut output_len, &mut error, &mut error_len,
            )
        };
        assert!(!output.is_null());
        assert!(error.is_null());
        assert_eq!(output_len, 1);
        let bytes = unsafe { std::slice::from_raw_parts(output.cast::<u8>(), output_len) };
        assert_eq!(bytes, b"\0");
        unsafe { reqflow_string_free(output, 1); }
        assert_eq!(error_len, 0);
    }

    #[test]
    fn ffi_returns_utf8_output_and_length() {
        let input = b"hola {{name}}";
        let variables = br#"{"name":"Rust"}"#;
        let mut output_len = 0;
        let mut error = ptr::null_mut();
        let mut error_len = 0;
        let output = unsafe {
            reqflow_expand_template(
                input.as_ptr().cast(), input.len(),
                variables.as_ptr().cast(), variables.len(), &mut output_len, &mut error, &mut error_len,
            )
        };
        assert!(!output.is_null());
        assert!(error.is_null());
        assert_eq!(output_len, 9);
        assert_eq!(unsafe { std::slice::from_raw_parts(output.cast::<u8>(), output_len) }, b"hola Rust");
        unsafe { reqflow_string_free(output, output_len); }
    }
}
