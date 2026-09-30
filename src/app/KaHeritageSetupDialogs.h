#pragma once
#include <QDialog>
#include <QList>

#include "core/HeritageFetchPlan.h"

class QCheckBox;
class QComboBox;
class QVBoxLayout;
struct HeritageRegion;

class KaHeritageAccountDialog : public QDialog {
  Q_OBJECT
public:
  explicit KaHeritageAccountDialog(QWidget* parent = nullptr);
};

// The one 시·군 confirm dialog of 주변유적 받기. The request unit stays one 시/군.
class KaHeritageRegionDialog : public QDialog {
  Q_OBJECT
public:
  explicit KaHeritageRegionDialog(const QString& sido = {}, const QString& city = {},
                                 const QString& reason = {}, QWidget* parent = nullptr);
  // Also lists the other 시/군 that the survey's 5 km scope touches (F120). Only the
  // resolved 시/군 is selected by default; ticked neighbours are fetched one after another.
  explicit KaHeritageRegionDialog(const HeritageRegion& region, const QString& reason = {},
                                 QWidget* parent = nullptr);
  QString sido() const;
  QString city() const;
  // Ticked neighbours and the 「최근 받은 자료 다시 쓰기」 choice. Pass to
  // KaHeritageBrowser::setFetchPlan after setTarget.
  HeritageFetchPlan plan() const;

private:
  void addNearby(const QList<HeritageCity>& nearby, const QString& note);
  void refreshReuse();

  QVBoxLayout* m_layout = nullptr;
  QComboBox* m_sido = nullptr;
  QComboBox* m_city = nullptr;
  QList<QCheckBox*> m_nearby;
  QCheckBox* m_reuse = nullptr;
};
