Name:           reqflow-ai
Version:        %{_version}
Release:        1%{?dist}
Summary:        Native desktop HTTP API client
License:        Unknown
URL:            https://github.com/CRISTOP-bot/ReqFlowAI
BuildArch:      x86_64
Requires:       qt6-qtbase

%description
ReqFlow AI is a local-first Qt desktop application for creating and sending
HTTP requests and managing API workflows.

%install
install -D -m 0755 %{_sourcedir}/reqflow-ai %{buildroot}%{_bindir}/reqflow-ai
install -D -m 0644 %{_sourcedir}/reqflow-ai.desktop %{buildroot}%{_datadir}/applications/reqflow-ai.desktop

%files
%{_bindir}/reqflow-ai
%{_datadir}/applications/reqflow-ai.desktop

%changelog
* Sun Oct 25 2026 ReqFlow AI contributors - 0.2.0-1
- Initial RPM package
