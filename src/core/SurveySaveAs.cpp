#include "SurveySaveAs.h"

#include "SurveyStorage.h"

#include <QFileInfo>

#include <qgsproject.h>
#include <qgsvectorlayer.h>

namespace SurveySaveAs {

bool moveToCopy(QgsProject* project, const QString& original, const QString& target, QString* error) {
  QString copyError;
  if (!project || !SurveyStorage::copySurvey(original, target, &copyError)) {
    if (error) *error = copyError;
    return false;
  }
  // 논리 키는 같은 구역도 실제 테이블은 survey_area_2 등으로 다를 수 있다.
  // 파일 경로만 바꾸고 각 레이어의 테이블·옵션·스타일은 보존한다.
  const QString originalPath = QFileInfo(original).absoluteFilePath();
  for (QgsMapLayer* l : project->mapLayers()) {
    auto* vl = qobject_cast<QgsVectorLayer*>(l);
    if (!vl || vl->providerType() != QLatin1String("ogr")) continue;
    const QString source = vl->source();
    if (QFileInfo(source.section(QLatin1Char('|'), 0, 0)).absoluteFilePath().compare(originalPath,
                                                                                     Qt::CaseInsensitive) != 0)
      continue;
    const int options = source.indexOf(QLatin1Char('|'));
    vl->setDataSource(target + (options < 0 ? QString() : source.mid(options)), vl->name(), QStringLiteral("ogr"));
    if (!vl->isValid()) {
      if (error) *error = QStringLiteral("새 파일에서 %1을 읽지 못했습니다.").arg(vl->name());
      return false;
    }
  }
  return true;
}

}  // namespace SurveySaveAs
