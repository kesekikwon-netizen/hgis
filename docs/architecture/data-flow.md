# ka-hgis data-flow graph

## Current critical path (2026-09-29)

```mermaid
flowchart LR
  Boot[KaApplication boot] --> Home[MainWindow home only<br/>no auto-restore]
  Home -->|새 조사| Factory[SurveyProjectFactory<br/>work CRS 5186/5187]
  Factory --> GPKG[(survey .gpkg<br/>7 domain tables + embedded workspace)]
  Home -->|열기| Open[SurveyStorage::readEmbedded<br/>+ LayerOps repair/remount]
  Open --> GPKG
  Ribbon[one-row ribbon] -->|그리기| Capture[KaCaptureMapTool<br/>no popup while drawing]
  Capture --> Ensure[LayerOps::ensureDomainLayer<br/>only path adding domain layers]
  Ensure --> GPKG
  Ribbon -->|Ctrl+S| Persist[SurveyStorage::persistWorkspace<br/>generation copy, verify, publish]
  Persist --> GPKG
  GPKG --> State[ProjectStateBuilder / ChecklistState]
  Rules[(drawing_checklist.v1.json)] --> Check[ChecklistEngine]
  State --> Check
  Check -->|error blocks| Export[ExportService::exportSubmissionPackage]
  Layout[LayoutService user_sheet] --> Export
  Export --> Pack[(EPSG:5179 SHP + 조사도면.pdf + MANIFEST.sha256)]
```

Domain tables: `survey_area`, `feature_poly`, `feature_line`, `section_line`, `control_points`, `artifact_point`, `trial_trench` (schema: `data/schemas/ka_hgis_layers.yaml`, checked against the factory by `workflow_engine`). Reference maps (VWorld, terrain, soil, heritage) are not survey data; downloaded and VWorld cadastral layers sit at the layer-tree root.

## Historical sprint graph (production sprint, 2026-08)

> Historical sprint overview. The 7-step UI, attribute gate and initial five-layer schema describe that sprint; they are not instructions to restore those flows. For current UI, lazy legend creation, explicit saving and supported domain keys, read `AGENTS.md`, `.codex/NOW.md` and current handoffs. Follow actual callers when changing the critical path.

```mermaid
flowchart LR
  Boot[KaApplication.boot] --> UI[MainWindow 7-step]
  UI -->|step0| Factory[SurveyProjectFactory]
  Factory --> GPKG[(GPKG 5 layers)]
  UI -->|step2-3| Digitize[QgsMapToolDigitizeFeature]
  Digitize --> GPKG
  UI -->|step4| GCP[control_points GNSS]
  GCP --> GPKG
  GPKG --> State[ProjectStateBuilder live]
  State --> Check[ChecklistEngine]
  Rules[(drawing_checklist.v1.json)] --> Check
  Check -->|pass| Pack[SHP package + Manifest]
  Check -->|pass| Layout[LayoutService PDF]
```

## Nodes
| Node | Role |
| --- | --- |
| KaApplication | QgsApplication init, prefix/PATH |
| MainWindow steps 0-6 | Beginner IA |
| SurveyProjectFactory | Create domain GPKG |
| survey_area, feature_poly, feature_line, section_line, control_points | Domain layers |
| ProjectStateBuilder | Live QJsonObject for checklist |
| ChecklistEngine | Rule eval |
| LayoutService | QgsPrintLayout + QgsLayoutExporter |
| ExportService / ShpPackage | SHP + README + MANIFEST.sha256 |

## Gaps closed by this sprint
| GAP | Node fix |
| --- | --- |
| Stub counters | ProjectStateBuilder |
| Digitize shallow | Digitize + attr gate |
| Fake PDF | LayoutService |
| Partial SHP | writeAsVectorFormatV3 package |
| Hardcode D:/qgis | rulesPath portable |

## Parallel P1
GNSS quality fields, topology/extent, hash manifest, e2e deepen
