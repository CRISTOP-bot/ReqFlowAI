#include "reqflow/testing/jsondiff.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <algorithm>

namespace reqflow::testing {
namespace {
QString escapeSegment(QString segment) {
    segment.replace(QLatin1Char('~'), QStringLiteral("~0"));
    segment.replace(QLatin1Char('/'), QStringLiteral("~1"));
    return segment;
}
void walk(const QJsonValue &before, const QJsonValue &after, const QString &path,
          int depth, int maxDepth, int maxChanges, JsonDiffResult *result) {
    if (result->changes.size() >= maxChanges) { result->truncated = true; return; }
    if (depth > maxDepth) {
        if (before != after) result->changes.append({path.isEmpty()?QStringLiteral("$"):path,JsonChangeKind::Modified,before,after});
        result->truncated = true;
        return;
    }
    if (before == after) return;
    if (before.isObject() && after.isObject()) {
        const auto lhs=before.toObject(), rhs=after.toObject();
        QSet<QString> keys;
        for(auto it=lhs.begin();it!=lhs.end();++it)keys.insert(it.key());
        for(auto it=rhs.begin();it!=rhs.end();++it)keys.insert(it.key());
        QStringList sorted=keys.values(); std::sort(sorted.begin(),sorted.end());
        for(const auto &key:sorted){
            if(result->changes.size()>=maxChanges){result->truncated=true;return;}
            const QString child=path+QStringLiteral(".")+escapeSegment(key);
            const bool inBefore=lhs.contains(key), inAfter=rhs.contains(key);
            if(!inBefore)result->changes.append({child,JsonChangeKind::Added,{},rhs.value(key)});
            else if(!inAfter)result->changes.append({child,JsonChangeKind::Removed,lhs.value(key),{}});
            else walk(lhs.value(key),rhs.value(key),child,depth+1,maxDepth,maxChanges,result);
        }
        return;
    }
    if(before.isArray()&&after.isArray()){
        const auto lhs=before.toArray(),rhs=after.toArray();const qsizetype common=qMin(lhs.size(),rhs.size());
        for(qsizetype i=0;i<common;++i){if(result->changes.size()>=maxChanges){result->truncated=true;return;}walk(lhs.at(i),rhs.at(i),path+QStringLiteral("[%1]").arg(i),depth+1,maxDepth,maxChanges,result);}
        for(qsizetype i=common;i<rhs.size();++i){if(result->changes.size()>=maxChanges){result->truncated=true;return;}result->changes.append({path+QStringLiteral("[%1]").arg(i),JsonChangeKind::Added,{},rhs.at(i)});}
        for(qsizetype i=common;i<lhs.size();++i){if(result->changes.size()>=maxChanges){result->truncated=true;return;}result->changes.append({path+QStringLiteral("[%1]").arg(i),JsonChangeKind::Removed,lhs.at(i),{}});}
        return;
    }
    result->changes.append({path.isEmpty()?QStringLiteral("$"):path,JsonChangeKind::Modified,before,after});
}
}

QString jsonValueForDiff(const QJsonValue &value) {
    if(value.isUndefined())return QStringLiteral("<missing>");
    if(value.isString())return value.toString();
    if(value.isObject())return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    if(value.isArray())return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    if(value.isNull())return QStringLiteral("null");
    return value.toVariant().toString();
}
QString jsonChangeKindName(JsonChangeKind kind) {
    switch(kind){case JsonChangeKind::Added:return QStringLiteral("Added");case JsonChangeKind::Removed:return QStringLiteral("Removed");case JsonChangeKind::Modified:return QStringLiteral("Modified");}
    return QStringLiteral("Unknown");
}
JsonDiffResult compareJson(const QJsonValue &before,const QJsonValue &after,int maxChanges,int maxDepth){
    JsonDiffResult result;if(maxChanges<1){result.truncated=true;return result;}walk(before,after,QString{},0,qMax(0,maxDepth),maxChanges,&result);return result;
}
} // namespace reqflow::testing
