#include "KaSessionLog.h"
#include "SurveyProjectFactory.h"
#include "KaSafeQgis.h"
#include "SurveyStorage.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <exception>

#include <qgscoordinatereferencesystem.h>
#include <qgsfield.h>
#include <qgsfields.h>
#include <qgsproject.h>
#include <qgsvectorfilewriter.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgscoordinatetransformcontext.h>

QString SurveyProjectFactory::createNewSurvey(const QString& directory,
                                              const QString& surveyName,
                                              QString* errorOut,
                                              const QString& workCrsAuthId) {
  if (errorOut) errorOut->clear();
  try {
    QDir dir(directory);
    if (!dir.exists() && !QDir().mkpath(directory)) {
      if (errorOut) *errorOut = QStringLiteral("저장 폴더를 만들 수 없습니다. 쓰기 가능한 다른 폴더를 선택해 주세요.");
      return {};
    }
    QString safe = surveyName.trimmed();
    if (safe.isEmpty()) safe = QStringLiteral("survey");
    safe.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));

    const QString gpkgPath = dir.filePath(safe + QStringLiteral(".gpkg"));
    const QString qgzPath = dir.filePath(safe + QStringLiteral(".qgz"));
    if (QFileInfo::exists(gpkgPath) || QFileInfo::exists(qgzPath)) {
      if (errorOut)
        *errorOut = QStringLiteral(
            "같은 이름의 조사 파일이 이미 있습니다. 기존 파일은 그대로 두었습니다. 다른 조사명이나 폴더를 선택해 "
            "주세요.");
      return {};
    }

    QString crsId = workCrsAuthId.trimmed();
    if (crsId.isEmpty()) crsId = QString::fromUtf8(defaultWorkCrsAuthId());
    QgsCoordinateReferenceSystem crs(crsId);
    if (!crs.isValid()) {
      if (errorOut)
        *errorOut =
            QStringLiteral("작업 좌표계를 확인할 수 없습니다. 중부원점(5186) 또는 동부원점(5187)을 선택해 주세요.");
      return {};
    }
    QTemporaryDir staging(dir.filePath(QStringLiteral(".ka-new-survey-XXXXXX")));
    if (!staging.isValid()) {
      if (errorOut)
        *errorOut =
            QStringLiteral("새 조사 파일을 준비할 수 없습니다. 저장 폴더의 쓰기 권한과 남은 공간을 확인해 주세요.");
      return {};
    }
    const QString stagedGpkg = staging.filePath(safe + QStringLiteral(".gpkg"));
    const QString stagedQgz = staging.filePath(safe + QStringLiteral(".qgz"));
    QgsCoordinateTransformContext transformContext;

    struct LayerDef {
      const char* name;
      const char* geom;
    };
    const LayerDef defs[] = {
        {"survey_area", "Polygon"},     {"feature_poly", "Polygon"}, {"feature_line", "LineString"},
        {"section_line", "LineString"}, {"control_points", "Point"}, {"artifact_point", "Point"},
        {"trial_trench", "Polygon"},
    };

    bool first = true;
    for (const LayerDef& d : defs) {
      const QString uri = QStringLiteral("%1?crs=%2").arg(QString::fromUtf8(d.geom), crs.authid());
      QgsVectorLayer mem(uri, QString::fromUtf8(d.name), QStringLiteral("memory"));
      if (!mem.isValid()) {
        if (errorOut) *errorOut = QStringLiteral("새 조사의 도형 저장 공간을 준비하지 못했습니다. 다시 시도해 주세요.");
        return {};
      }
      QgsFields fields;
      const QString n = QString::fromUtf8(d.name);
      if (n == QLatin1String("survey_area")) {
        fields.append(QgsField(QStringLiteral("survey_name"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("site_name"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
      } else if (n == QLatin1String("feature_poly")) {
        fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("period"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("feature_no"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
      } else if (n == QLatin1String("feature_line")) {
        fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("period"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
      } else if (n == QLatin1String("section_line")) {
        fields.append(QgsField(QStringLiteral("section_id"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
      } else if (n == QLatin1String("artifact_point")) {
        fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("period"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("artifact_no"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("note"), QMetaType::Type::QString));
      } else if (n == QLatin1String("trial_trench")) {
        fields.append(QgsField(QStringLiteral("name"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("width"), QMetaType::Type::Double));
        fields.append(QgsField(QStringLiteral("length"), QMetaType::Type::Double));
      } else {
        fields.append(QgsField(QStringLiteral("point_id"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("x"), QMetaType::Type::Double));
        fields.append(QgsField(QStringLiteral("y"), QMetaType::Type::Double));
        fields.append(QgsField(QStringLiteral("z"), QMetaType::Type::Double));
        fields.append(QgsField(QStringLiteral("datum"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("ellipsoid"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("projection"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("origin"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("accuracy"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("accuracy_m"), QMetaType::Type::Double));
        fields.append(QgsField(QStringLiteral("pdop"), QMetaType::Type::Double));
        fields.append(QgsField(QStringLiteral("fix_type"), QMetaType::Type::QString));
        fields.append(QgsField(QStringLiteral("pixel_x"), QMetaType::Type::Double));
        fields.append(QgsField(QStringLiteral("pixel_y"), QMetaType::Type::Double));
      }
      if (!mem.dataProvider()->addAttributes(fields.toList())) {
        if (errorOut) *errorOut = QStringLiteral("새 조사의 기록 항목을 준비하지 못했습니다. 다시 시도해 주세요.");
        return {};
      }
      mem.updateFields();
      mem.setCrs(crs);

      QgsVectorFileWriter::SaveVectorOptions opts;
      opts.driverName = QStringLiteral("GPKG");
      opts.layerName = n;
      opts.fileEncoding = QStringLiteral("UTF-8");
      opts.actionOnExistingFile =
          first ? QgsVectorFileWriter::CreateOrOverwriteFile : QgsVectorFileWriter::CreateOrOverwriteLayer;
      first = false;

      QString errorMessage;
      QString newFilename;
      QString newLayer;
      const auto err = QgsVectorFileWriter::writeAsVectorFormatV3(&mem, stagedGpkg, transformContext, opts,
                                                                  &errorMessage, &newFilename, &newLayer);
      if (err != QgsVectorFileWriter::NoError) {
        if (errorOut)
          *errorOut =
              QStringLiteral(
                  "새 조사 파일을 기록하지 못했습니다. 남은 공간과 쓰기 권한을 확인한 뒤 다시 시도해 주세요.\n%1")
                  .arg(errorMessage);
        return {};
      }
    }

    QString validationError;
    if (!SurveyStorage::validateForOpen(stagedGpkg, &validationError)) {
      if (errorOut)
        *errorOut = QStringLiteral("새 조사 파일을 확인하지 못해 생성을 중단했습니다. 다시 시도해 주세요.\n%1")
                        .arg(validationError);
      return {};
    }

    QgsProject proj;
    proj.setCrs(crs);
    proj.setTitle(safe);
    // The schema exists on disk, but a new survey has no legend layers until drawing/import.
    QString workspaceError;
    if (!kaWriteQgisProjectAtomic(&proj, stagedQgz, &workspaceError)) {
      if (errorOut)
        *errorOut =
            QStringLiteral("새 조사의 작업 화면을 저장하지 못했습니다. 남은 공간과 쓰기 권한을 확인해 주세요.\n%1")
                .arg(workspaceError);
      return {};
    }

    // Same-volume rename never overwrites an existing destination, including one
    // created by another process after the initial existence check.
    bool installedGpkg = false;
    const auto removeIncompletePair = qScopeGuard([&] {
      if (installedGpkg) QFile::remove(gpkgPath);
    });
    if (!QFile::rename(stagedGpkg, gpkgPath)) {
      if (errorOut)
        *errorOut = QStringLiteral(
            "새 조사 파일을 저장 폴더에 놓을 수 없습니다. 같은 이름의 파일이 생겼는지 확인하고 다른 이름으로 다시 "
            "시도해 주세요.");
      return {};
    }
    installedGpkg = true;
    if (!QFile::rename(stagedQgz, qgzPath)) {
      const bool removed = QFile::remove(gpkgPath);
      installedGpkg = false;
      if (errorOut)
        *errorOut = removed ? QStringLiteral(
                                  "새 조사의 작업 화면을 저장 폴더에 놓을 수 없어 생성을 취소했습니다. 다른 이름이나 "
                                  "폴더를 선택해 주세요.")
                            : QStringLiteral(
                                  "새 조사의 작업 화면을 저장하지 못했습니다. 새로 만든 조사 파일은 %1에 남아 "
                                  "있습니다. 기존 조사를 계속 사용하고 이 파일은 별도로 확인해 주세요.")
                                  .arg(gpkgPath);
      return {};
    }
    installedGpkg = false;
    return gpkgPath;
  } catch (const std::exception& ex) {
    if (errorOut)
      *errorOut = QStringLiteral(
                      "새 조사 준비 중 오류가 발생했습니다. 기존 조사는 그대로 두었습니다. 다른 이름이나 폴더로 다시 "
                      "시도해 주세요.\n%1")
                      .arg(QString::fromUtf8(ex.what()));
    return {};
  } catch (...) {
    KaSessionLog::line(QStringLiteral("[except] core/SurveyProjectFactory.cpp:217"));
    if (errorOut)
      *errorOut = QStringLiteral(
          "새 조사 준비 중 오류가 발생했습니다. 기존 조사는 그대로 두었습니다. 다른 이름이나 폴더로 다시 시도해 "
          "주세요.");
    return {};
  }
}
