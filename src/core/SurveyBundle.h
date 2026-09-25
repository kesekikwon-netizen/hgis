#pragma once

#include <QString>
#include <QStringList>

class QgsProject;

// 조사 폴더 하나만 옮기면 다른 PC·새 포터블에서도 조사가 그대로 열리게 하는 규칙.
//
// 앱이 스스로 받거나 만든 자료(주변유적, 합친 수치지형도 등)는 AppData·임시 폴더·앱
// 폴더에 생긴다. 그런 파일은 이 PC에만 있어 조사 폴더를 옮기면 따라가지 않고, 새
// 포터블은 새 폴더라 앱 폴더에 있던 파일도 두고 온다. 저장할 때 이런 파일을 조사
// 폴더의 「가져온자료」로 모으고, 열 때는 저장한 조사 폴더 기준으로 경로를 옮겨 붙인다.
namespace SurveyBundle {

// 조사 폴더 안에 모은 자료를 두는 하위 폴더 이름.
QString collectedFolderName();

// 앱이 만든 자료가 놓이는 곳인가: AppData(앱 자료·캐시), 임시 폴더, 앱(포터블) 폴더,
// 저장용 세대 폴더(.ka-survey-gen-*).
bool isAppManagedPath(const QString& path);

// 없으면 다시 만드는 캐시(DEM 음영 .vrt 등). 옮기지 않는다.
bool isRegeneratedCache(const QString& path);

struct CollectResult {
  QStringList copied;  // 조사 폴더로 모아 온 레이어 이름
  QStringList failed;  // 모으지 못한 레이어 이름(원래 경로 그대로 둔다)
};

// 앱이 만든 바깥 파일을 가리키는 레이어를 조사 폴더의 「가져온자료」로 복사하고 그 사본을
// 가리키게 한다. 사용자가 직접 고른 바깥 파일(예: D:/항공사진)은 크기를 알 수 없어 두고,
// 열 때 못 찾으면 알린다. previousSurveyDir 를 주면(다른 이름으로 저장) 이전 조사 폴더에
// 있던 파일도 같은 하위 폴더 구조로 새 조사 폴더에 옮겨 온다.
CollectResult collectIntoSurvey(QgsProject* project, const QString& surveyDir,
                                const QString& previousSurveyDir = QString());

// 저장 때 적어 두는 조사 폴더(절대 경로). 옮긴 뒤 열 때 경로를 옮겨 붙이는 기준이다.
QString savedSurveyDir(const QgsProject* project);
void rememberSurveyDir(QgsProject* project, const QString& surveyDir);

// savedDir 아래를 가리키는 path 를 currentDir 아래로 옮겨 붙인다. 해당 없으면 빈 문자열.
// path 뒤의 |layername= 같은 꼬리는 그대로 둔다.
QString rebaseIntoSurvey(const QString& path, const QString& savedDir, const QString& currentDir);

// USB 드라이브 글자가 바뀐 경우(E: → F:): source 의 드라이브를 조사 폴더가 지금 있는 드라이브로
// 바꾼다. 조사 폴더와 자료를 같은 USB 에 두고 다니면 이것으로 붙는다. 해당 없으면 빈 문자열.
QString onSurveyDrive(const QString& source, const QString& surveyDir);

// 레이어 원본에서 파일 경로만 뽑는다(/vsizip/·GPKG: 앞붙이와 |layername= 꼬리를 뗀다).
QString sourceFile(const QString& source);

// 조사를 연 뒤에도 원본 파일을 못 찾은 레이어 이름. 다시 만드는 캐시는 뺀다.
QStringList missingFileLayers(QgsProject* project);

}  // namespace SurveyBundle
