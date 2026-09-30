#pragma once

#include <QDialog>
#include <QPointer>

class QgsProject;
class QLabel;
class QTreeWidget;

// 「자료 준비 상태」: a read-only table of the reference data this survey uses
// (kind, area, received date, size, works offline or not). The user opens it
// explicitly; it never downloads, loads or removes anything.
class KaReferenceStatusDialog final : public QDialog {
  Q_OBJECT
public:
  explicit KaReferenceStatusDialog(QgsProject* project, QWidget* parent = nullptr);
  void refresh();

private:
  QPointer<QgsProject> m_project;
  QTreeWidget* m_table = nullptr;
  QLabel* m_summary = nullptr;
};
