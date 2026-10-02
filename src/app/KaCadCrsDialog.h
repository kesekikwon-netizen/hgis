#pragma once

#include <QDialog>

#include <functional>
#include <optional>

#include "core/CadCrsGuess.h"

class QListWidget;

// 좌표계 정보가 없는 CAD 도면의 좌표계 후보를 「경상북도 부근 · …」처럼 지역 이름과 함께 보여 주고 고르게 한다.
class KaCadCrsDialog : public QDialog {
  Q_OBJECT
 public:
  explicit KaCadCrsDialog(const CadCrsResult& guess, QWidget* parent = nullptr);

  // 단추를 누른 뒤의 결과: 0 이상 = 후보 줄, -1 = 좌표 없는 도면(직접 맞추기), nullopt = 취소·아직
  std::optional<int> outcome() const { return m_outcome; }

  // 시험용 고르기 함수가 있으면 그것을 쓰고, 없으면 창을 띄워 고르게 한다.
  static std::optional<int> choose(QWidget* parent, const CadCrsResult& guess);
  static void setChooserForTests(std::function<std::optional<int>(const CadCrsResult&)> chooser);  // 빈 함수면 되돌림

 private:
  QListWidget* m_list = nullptr;
  std::optional<int> m_outcome;
};
