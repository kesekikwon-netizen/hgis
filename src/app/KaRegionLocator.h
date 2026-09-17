#pragma once

#include <QWidget>
#include <QString>
#include <QList>

class QComboBox;
class QLineEdit;
class QLabel;
class QFrame;
class QButtonGroup;
class QAbstractButton;
class QGridLayout;

class KaRegionLocator : public QWidget {
  Q_OBJECT
public:
  explicit KaRegionLocator(QWidget* parent = nullptr);
  QSize sizeHint() const override;

signals:
  void regionSelected(const QString& sido);
  void searchRequested(const QString& query);
  void parcelSearchRequested(const QString& query);

protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

private:
  friend class RegionLocatorTest;
  void openAddressPopup(const QString& sido);
  void placeAddressPopup(const QRect& anchor, const QRect& available);
  void fillDongs();
  void emitSearch();
  // 팝업을 닫고 시·도 칩도 함께 해제한다. 팝업 열림 == 칩 눌림이 유일한 규칙.
  void closePanel();

  QButtonGroup* m_group = nullptr;
  QFrame* m_popup = nullptr;
  QGridLayout* m_addressLayout = nullptr;
  QList<QWidget*> m_addressControls;
  QLabel* m_sidoLabel = nullptr;
  QComboBox* m_city = nullptr;
  QComboBox* m_dong = nullptr;
  QLineEdit* m_lot = nullptr;
  QString m_sido;
  QAbstractButton* m_activeChip = nullptr;
};
