#pragma once

#include <QPointer>
#include <QString>
#include <QUndoCommand>
#include <QUndoStack>

#include <qgsvectorlayer.h>

// 사용자 동작 하나(도형 그리기와 이름·번호)를 레이어 되돌리기 목록의 한 단계로 묶는다: Ctrl+Z·Ctrl+Y 한 번.
// 만들 때 편집을 켜고 묶음을 열며, close() 나 소멸 때 닫는다. 안에 편집이 없으면 단계를 남기지 않는다.
// 따로 한 단계로 남겨야 하는 편집(「겹친 곳 지우기」)을 하기 전에는 close() 한다.
class KaUndoGroup {
 public:
  KaUndoGroup(QgsVectorLayer* layer, const QString& title) {
    if (layer && (layer->isEditable() || layer->startEditing()) && layer->undoStack()) {
      m_stack = layer->undoStack();
      m_stack->beginMacro(title);
    }
  }
  ~KaUndoGroup() { close(); }
  KaUndoGroup(const KaUndoGroup&) = delete;
  KaUndoGroup& operator=(const KaUndoGroup&) = delete;

  void close() {
    if (!m_stack) return;
    QUndoStack* stack = m_stack;
    m_stack = nullptr;
    stack->endMacro();
    auto* group = const_cast<QUndoCommand*>(stack->command(stack->index() - 1));
    if (group && group->childCount() == 0) {  // 빈 단계는 되돌리기·다시 하기 어디에도 남기지 않는다
      group->setObsolete(true);
      stack->undo();
    }
  }

 private:
  QPointer<QUndoStack> m_stack;
};
