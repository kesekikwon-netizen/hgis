#pragma once

#include <QString>
#include <QStringList>

class QgsProject;
class QgsMapLayer;
enum class HeritageDataset;

// The intranet 「원본자료 사용 서약서」 (evaluation F169).
//
// The user decided the pledge is agreed automatically and its time and full text are kept
// as a receipt. This does not change that. It only makes the pledge visible: a short
// disclosure in the one confirm dialog, the pledge's own use/redistribution sentences
// after agreeing, and a checklist guidance when intranet data is in the project.
// Nothing here is invented pledge content: highlights are quoted from the captured text.
namespace HeritagePledge {

// Shown in the 시·군 confirm dialog before anything is fetched.
QString preDisclosure();

// Sentences of the captured pledge text about use, export, redistribution or publishing,
// in their original wording, at most maxLines, each shortened to maxChars.
QStringList highlights(const QString& termsText, int maxLines = 3, int maxChars = 110);

// One notice for the progress window right after the automatic agreement.
QString noticeAfterAgreement(const QString& termsText);

// Intranet layers carry the logical dataset tag of HeritageLayoutNumbers
// ("ka_hgis/heritage_dataset", set by HeritageStyle::apply). This keeps that tag as it is
// (layout numbers read it) and adds the 시/군 of a multi-city run as "ka_hgis/heritage_region".
void markIntranetLayer(QgsMapLayer* layer, HeritageDataset dataset, const QString& regionLabel);
bool isIntranetLayer(const QgsMapLayer* layer);
bool projectHasIntranetLayers(const QgsProject* project);

// Submit checklist: state key (ProjectStateBuilder) and whether the guidance rule passes.
// The rule text lives in data/rules/drawing_checklist.v1.json like every other rule.
QString checklistStateKey();  // "HERITAGE_PLEDGE_SCOPE"
bool checklistPasses(const QgsProject* project);

}  // namespace HeritagePledge
