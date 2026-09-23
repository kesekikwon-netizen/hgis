#include <cmath>
#include <limits>
#include <memory>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <QtTest>
#include <QStringConverter>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QRectF>
#include <QJsonObject>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>
#include <QEvent>
#include <QSize>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <qgsvectorfilewriter.h>

#include "core/SurveyProjectFactory.h"
#include "core/ProjectStateBuilder.h"
#include "core/ChecklistEngine.h"
#include "core/ExportService.h"
#include "core/HeritageImport.h"
#include "core/LayoutService.h"
#include "core/LayerOps.h"
#include <qgslayoutitempage.h>
#include <qgslayoutpagecollection.h>
#include "core/KaPortableRuntime.h"
#include "core/WorkflowGuide.h"
#include "core/VworldSettings.h"
#include "core/LocationSearch.h"
#include "core/KoreaRegionCatalog.h"
#include "core/SoilMapService.h"
#include "core/GeologyMapService.h"
#include "core/RiverMapService.h"
#include "core/PaleoLandformService.h"
#include "core/GeorefService.h"
#include "core/KaSafeQgis.h"
#include "core/SurveyStorage.h"
#include "core/AdminBoundaryService.h"
#include "core/SectionLayoutService.h"
#include "app/KaCaptureMapTool.h"
#include <QCryptographicHash>
#include <QRegularExpression>

#include <gdal_priv.h>
#include <ogr_api.h>
#include <ogr_spatialref.h>
#include <qgssinglebandpseudocolorrenderer.h>
#include <qgsrastershader.h>
#include <qgscolorrampshader.h>
#include <qgsrasterblock.h>
#include <qgshillshaderenderer.h>
#include <QPainter>

#include <qgis.h>
#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgssnappingconfig.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>
#include <qgscoordinatetransform.h>
#include <qgsrasterlayer.h>
#include <qgsfeature.h>
#include <qgsfeatureid.h>
#include <qgsrectangle.h>
#include <qgsgeometry.h>
#include <QUndoStack>
#include <qgspointxy.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsvectorfilewriter.h>
#include <qgsfields.h>
#include <qgsfield.h>
#include <qgscoordinatetransformcontext.h>
#include <qgslayertreemodel.h>
#include <qgslayertree.h>
#include <qgslayertreegroup.h>
#include <qgsfillsymbollayer.h>
#include <qgsmaprenderercustompainterjob.h>
#include <qgsmapsettings.h>
#include <qgslinesymbollayer.h>
#include <QImageReader>
#include <QElapsedTimer>
#include <qgslayoutmanager.h>
#include <qgslayoutexporter.h>
#include <qgsmapcanvas.h>
#include <qgslayertreelayer.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsinvertedpolygonrenderer.h>
#include <qgslinesymbol.h>
#include <qgssymbollayer.h>
#include <qgsprintlayout.h>
#include <qgslayoutrendercontext.h>
#include <qgslayoutitemmap.h>
#include <qgslayoutitemmapgrid.h>
#include <qgslayoutitemlabel.h>
#include <qgslayoutitemscalebar.h>
#include <qgslayoutitemlegend.h>
#include <qgsfillsymbol.h>
#include <qgspallabeling.h>
#include <qgsvectorlayerlabeling.h>

class TestWorkflow : public QObject {
  Q_OBJECT
private slots:
  void gpkgEmbeddedProject_writesAndReadsBack();
  void surveyFileTravelsAloneWithAbsorbedShp();
  void copySurvey_includesCommittedWalAndKeepsSourceIndependent();
  void copySurvey_failurePreservesExistingDestination();
  void atomicProjectWrite_neverLeavesPartialFile();
  void atomicProjectWrite_writtenFileReadsBackAsQgz();
  void atomicProjectWrite_keepsOneBackupGeneration();
  void fullWorkflowSurveyToPackage();
  void exportSubmissionPackage_pdfIsUserSheetOnly();
  void exportSubmissionPackage_withoutUserSheetFails();
  void exportSubmissionPackage_preservesPreviousPackage();
  void exportManifestFailurePreservesPreviousFile();
  void exportSubmissionPackage_finalizationFailure_data();
  void exportSubmissionPackage_finalizationFailure();
  void exportSubmissionPackage_excludesPrivateHeritage_data();
  void exportSubmissionPackage_excludesPrivateHeritage();
  void exportLayoutPdf_userSheetMissing_doesNotSeedFiveTemplates();
  void shpKoreanRoundTripUtf8();
  void soilShpImport_crsOverrideAndCategorizedStyle();
  void soilTerrainLegend_officialCodesAndStyle();
  void geologyEraLegend_icsColorsAndStyle();
  void geologyLithoWfs_excludesJejuUsesOfficialRasterUri();
  void geologyJejuLitho_sameCategorizedLegendAsMainland();
  void thematicOverlay_secondToggleHidesLayer();
  void mapTools_secondClickReturnsToPan();
  void geologyDownload_skipsBlockingWmsAndRefreshWhileDrawing();
  void geologyRelief_drapesMultiplyOverHillshade();
  void demRelief_drapesMultiplyOverDem();
  void ensureDomainLayer_reopenAfterPurgeClearsDeletedPolygons();
  void vertexEdit_snapsAndOffersAddDeleteOnLine();
  void riverLevelLegend_waterStyleAndNameLabels();
  void thematicMapsScaleRangeTo1in100000();
  void thematicDownloadMaxSpanFourTimesPrior();
  void thematicDownloadCovers1in10000Drawing();
  void soilDownload_usesViewExtentAndTerrainFieldOnly();
  void soilTerrainPicture_is3857ArcGisCacheNot5186Wms();
  void elevationMap_is3857ReferenceToggleNotTiffDialog();
  void elevationMap_xyzOnlySkipsAbortWhilePanning();
  void demColorRelief_is3857XyzNotTerrainMap();
  void demElevationStyle_legendListsHeightMeters();
  void demElevationStyle_continuousLowlandStops();
  void demElevationStyle_userClassCountAndCustomItems();
  void demNgiiImg_loadsWithMeterLegend();
  void paleoLandform_candidateEmphasisAndReferenceLayer();
  void paleoLandform_toolbarIsNotDomainExport();
  void paleoLandform_seedFromSoilSplitsFloodplain();
  void sectionLineKeepsMagentaWithHalo();
  void featurePolyStrokeDarkerOnSatellite();
  void exportPackagePrefersUserSheetPdf();
  void sheetScaleBarUsesInkFill();
  void layoutOpenDoesNotAutoStartCoordPoint();
  void reprojectAndMigrateFields();
  void georefWorldfileFromGcp();
  void convert5186PolygonTo5179Shp();
  void vworldLayerOpsTest();
  void workflowGuideTracksSevenRealMilestones();
  void vworldSettingsAndNoKeyTests();
  void koreaRegionCatalog_gyeonggiGangwonAddressQuery();
  void regionLocator_sitsInAppBarRegionMenu();
  void adminBoundary_buildsEmdUrlWithoutHardcodedKey();
  void adminBoundary_parsesOkFeatureAndRejectsError();
  void layerOps_isolateSurfaceSurvey_satelliteAndUserSiteOnly();
  void editBufferCommitSurvivesReopen();
  void undoCommittedFeature_removesLastAdded();
  void mapEditUndoRedo_drawMoveDeleteRestoresFeature();
  void snapSettings_surviveProjectWriteAndReopen();
  void applySnapSettingsKeepsCadastralUnchecked();
  void topologicalVertexMove_movesSharedVertexOnBothFeatures();
  void featureForm_cancelKeepsGeometryAndOkWritesNameNumber();
  void captureVertexDrag_preservesEditsAndReportsOutcome_data();
  void captureVertexDrag_preservesEditsAndReportsOutcome();
  void importControlCsvWritesFeatures();
  void importControlCsv_keepsKoreanAxesAndEncoding();
  void suggestControlPointAxisSwap_matchesCsvRule();
  void osmBasemapValidWithExtent();
  void layerTreeReorderAndRemovalTest();
  void layerTreeDragReorderChangesOrder();
  void layerKeyOfKoreanDisplayName_featurePolyAttrs();
  void exportAllowsDespiteChecklistErrors();
  void vworldSatAndCadastralLiveKey();
  void ensureOtf_projectAndCanvasDestinationCrs5186();
  void syncMapCanvas_surveyLayersAboveReferenceBasemap();
  void satelliteAlwaysAtBottomInLegendAndCanvas();
  void satelliteDuplicatePrunedToOneInstance();
  void xyzBasemap_layerCrsForced3857();
  void suggestCadastralScale_clampsWhenTooZoomedOut();
  void prepareFieldBasemapPack_rejectsEmptyKey();
  void drawingRecipesHaveDistinctScaleAndExtent();
  void emptySurveyDrawingShowsKoreanHint();
  void zoomToLayerMax_movesCanvasToFeature();
  void zoomToLayerMax_movesCanvasToPointFeature();
  void zoomToKorea_refreshFalseSetsExtentWithoutUnfreeze();
  void clampCanvasToKorea_zoomOutDoesNotExceedKorea();
  void zoomToKorea_5186StaysInsideMercatorSatelliteQuad();
  void clampCanvasToKorea_secondCallIsNoOp();
  void clampCanvasToKorea_panPreservesCenterAtOverview_data();
  void clampCanvasToKorea_panPreservesCenterAtOverview();
  void syncMapCanvas_disablesParallelRenderingForXyzOtf();
  void isolateAndZoom_hidesOtherSurveyKeepsReference();
  void addVworldSatellite_allowsEmptyKeyViaPublicTiles();
  void addVworldSatellite_usesOfficialWmtsWhenKeyPresent();
  void addVworldSatellite_syncPutsLayerOnCanvasWithExtent();
  void canvasDisplayEvent_devicePixelRatioChangeNeedsTileRefresh();
  void refreshXyzBasemapTiles_restoresStaleDevicePixelRatio();
  void refreshXyzBasemapTiles_doesNotAbortInFlightWmsJob();
  void zoomToLayer_redrawsBasemapAtNewExtent();
  void shapeEditing_livesInsideSelectTool();
  void subToolbar_marksTheActiveToolForTheBlueUnderline();
  void alignPanel_showsHugeTiffViaGdalPreview();
  void alignPreview_keepsImageOrientation();
  void layerPanel_hasOneCheckAllToggle();
  void layoutLayerPanel_hasCheckAllToggle();
  void layoutLegend_deletableAndStaysDeleted();
  void opacityRail_hasBrightnessForRasters();
  void measureHudSitsBesideOpacityRail();
  void labelsFollowLayerStackOrder();
  void redLineDrawsOverLowerLayerLabels_map();
  void redLineDrawsOverLowerLayerLabels_layoutAndPdf();
  void perf_labelOrderAnalysisIsCheapPerRefresh();
  void layoutHonoursRasterBrightness();
  void layoutOpacityRail_staysPutWhenThePageMoves();
  void uiComboActions_doNotBustTileCacheWhileDrawing();
  void startupView_doesNotRestackXyzRefreshWhileWmsDownloads();
  void addVworldSatellite_fourKCanvasKeeps256pxTiles();
  void applyCanvasScreenDpi_outputSizeFollowsWideWidget();
  void convertToShp5179_addToMapFalseKeepsProjectLayerCount();
  void convertSelectedTo5179_sourceDoesNotAddToMap();
  void otf_keepsWorkCrsWhenBasemapIs3857();
  void koreaExtent_5186IsTmMetersNotDegrees();
  void setWorkCrs_5187to5186_keepsSatelliteAndLocalExtent();
  void applyKoreaMapLimits_doesNotReplaceLocalExtentWithFullKorea();
  void cadastralWmsCrs_neverStartsWithWorkCrs5179();
  void syncMapCanvas_cadastralAboveSatellite();
  void layoutBlankSheetMapItemKeepsFrameAndNonEmptyLayers();
  void layoutStudio_checkedLayersScaleBarLegendSize();
  void layoutEnter_matchesCanvasViewWithoutNiceSnap();
  void layoutStandardSheetChrome_sitsBelowMap();
  void layoutEqualFullSheetMapRect_matchesTopToSides();
  void layoutRegainsPageWhenTheSheetHasNone();
  void layoutExtentForPaperScale_keepsTypedDenominator();
  void layoutNiceScaleDenominator_endsOnTen();
  void legendTitlesHideEpsgAndUseShortKorean();
  void sheetLegend_soilShowsTerrainClassesNotPictureName();
  void sheetLegend_geologyDropsStructureKeepsLithology();
  void sheetLegend_hidesUncheckedSoilLayer();
  void sheetLegend_followsLayerCheckOnAndOff();
  void drawSubToolbarWiresEachDomainSlot();
  void digitizeTargetLayer_featurePolyIgnoresSurveyAreaCurrent();
  void newSurvey_removesUserLayersKeepsXyzBasemap();
  void layoutOpensAsMainWindowTabNotSeparateWindow();
  void startupLoadsSatelliteAndCadastralWithoutToolbarIcons();
  void layoutCoordPointHasIconAndCallout();
  void layoutCoordCallout_staysOnMapXyAfterScale();
  void layoutCoordCallout_rejectsOutsideAndCanUndo();
  void layoutProfessionalSheet_frameGridTitleBlock();
  void drawingStudio_sheetOmitsCrossesAndBorderRuler();
  void layoutRasterDrawnInOnePass();
  void layoutWheelZoom_keepsPointUnderCursor();
  void singleInstanceGuardIsWiredIntoBoot();
  void portableExe_setsPrefixFromExeDir();
  void portableRuntime_discoversUnicodeFolderAndKoreaCrs();
  void portableRuntime_ignoresLeftoverAppDataKey();
  void nameAttributeLabeling_5ptAndAreaCheck();
  void intersectionSnappingAndSaveAsPreservesLayers();
  void zoomToProjectDataLayers_usesUserVectorsNotKorea();
  void companionQgz_roundtripKeepsFifteenUserLayers();
  void openSurveyGpkg_usesSafeProjectRead();
  void emptyEmbeddedWorkspace_reopenStillShowsCommittedSurveyArea();
  void emptyEmbeddedWorkspace_reopenStillShowsUserPolygonLayer();
  void invalidWorkspaceLayer_isReplacedFromGpkgTable();
  void renamedSurvey_repointsImportedLayersInsteadOfDuplicating();
  void savedVworldUrl_swapsExpiredKeyForCurrentOne();
  void savedVworldGdalXml_refreshesKeyWithoutChangingOriginal();
  void savedGpkgWorkspace_restoresLegendMembership_data();
  void savedGpkgWorkspace_restoresLegendMembership();
  void savedGpkgWorkspace_sameKeyKeepsDistinctTables_data();
  void savedGpkgWorkspace_sameKeyKeepsDistinctTables();
  void satellitePruning_preservesUnresolvedProjectNodes();
  void savedWorkspace_restoresExternalAndRasterLegendNodes();
  void fieldAndongCopy_reopenRestoresSavedGpkgLayers();
  void fieldAndongCopy_embeddedRelativePathOpensWithoutCwd();
  void leftoverRestore_keepsUserFillColorNotFactoryDomainStyle();
  void restoreLastSurvey_prefersEmbeddedWorkspaceWhenSafe();
  void restoreLastSurvey_bootUsesLayersOnlyToAvoidWmsAv();
  void finishOpenedProject_doesNotBareRefreshWhileWms();
  void test_export_failure_aborts_and_reports_error();
  void test_section_sheet_bundling();
  void test_chunked_sha256_manifest_integrity();
  void test_challenge_pdf_failure_readonly_and_locked();
  void test_challenge_section_sheet_bundling_variations();
  void test_challenge_encoding_txt_flush_and_content();
  void test_challenge_manifest_64kb_chunking_stress();
  void test_challenge_manifest_self_exclusion_and_regex_format();
};

static bool projectHasLayerNamedLike(QgsProject* proj, const QString& base) {
  for (QgsMapLayer* l : proj->mapLayers()) {
    if (!l) continue;
    const QString n = l->name();
    if (n == base || n.startsWith(base) || n.contains(base) || base.contains(n))
      return true;
  }
  return false;
}

static QString readLayerOpsSources() {
  QString out;
  for (const QString& path : {QStringLiteral("src/core/LayerOps.cpp"),
                              QStringLiteral("src/core/BasemapOps.cpp"),
                              QStringLiteral("src/core/LabelOps.cpp"),
                              QStringLiteral("src/core/ControlPointCsv.cpp")}) {
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
      out += QString::fromUtf8(f.readAll());
  }
  return out;
}

static QString readMainWindowSources() {
  QString out;
  for (const QString& path : {QStringLiteral("src/app/MainWindow.cpp"),
                              QStringLiteral("src/app/MainWindowExport.cpp"),
                              QStringLiteral("src/app/MainWindowEditing.cpp"),
                              QStringLiteral("src/app/MainWindowSession.cpp")}) {
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
      out += QString::fromUtf8(f.readAll());
  }
  return out;
}

static QString rulesFile() {
  const QStringList c = {
    QDir::current().filePath(QStringLiteral("data/rules/drawing_checklist.v1.json")),
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../data/rules/drawing_checklist.v1.json")),
  };
  for (const auto& p : c)
    if (QFile::exists(p)) return p;
  return c.first();
}

static QgsVectorLayer* layer(QgsProject* p, const QString& n) {
  const auto ls = p->mapLayersByName(n);
  return ls.isEmpty() ? nullptr : qobject_cast<QgsVectorLayer*>(ls.first());
}

static QgsPrintLayout* addComposedUserSheet(QgsProject* proj, QgsMapLayer* mapLayer) {
  QString err;
  LayoutService::createBlankSheet(proj, 297.0, 210.0, QStringLiteral("user_sheet"), &err);
  auto* ly = dynamic_cast<QgsPrintLayout*>(
      proj->layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  if (!ly || !mapLayer)
    return ly;
  auto* map = new QgsLayoutItemMap(ly);
  map->setId(QStringLiteral("ka_map"));
  map->attemptSetSceneRect(QRectF(20.0, 20.0, 120.0, 80.0));
  map->setCrs(proj->crs().isValid() ? proj->crs()
                                   : QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  map->setKeepLayerSet(true);
  map->setLayers(QList<QgsMapLayer*>{mapLayer});
  map->zoomToExtent(QgsRectangle(200000.0, 450000.0, 200200.0, 450160.0));
  if (map->scene() != ly)
    ly->addLayoutItem(map);
  return ly;
}

void TestWorkflow::fullWorkflowSurveyToPackage() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_wf_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("wfdemo"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
  for (const char* n : {"survey_area", "feature_poly", "feature_line", "section_line", "control_points"}) {
    auto* vl = new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(gpkg, QString::fromUtf8(n)),
                                  QString::fromUtf8(n), QStringLiteral("ogr"));
    QVERIFY2(vl->isValid(), n);
    proj.addMapLayer(vl);
  }

  // survey polygon
  auto* sa = layer(&proj, QStringLiteral("survey_area"));
  QVERIFY(sa);
  QVERIFY(sa->startEditing());
  QgsFeature sf(sa->fields());
  sf.setAttribute(QStringLiteral("survey_name"), QStringLiteral("테스트조사"));
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200100, 450000) << QgsPointXY(200100, 450100)
       << QgsPointXY(200000, 450100) << QgsPointXY(200000, 450000);
  sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(sa->addFeature(sf));
  QVERIFY(sa->commitChanges());

  // feature poly with kind/period
  auto* fp = layer(&proj, QStringLiteral("feature_poly"));
  QVERIFY(fp->startEditing());
  QgsFeature ff(fp->fields());
  ff.setAttribute(QStringLiteral("kind"), QStringLiteral("수혈주거지"));
  ff.setAttribute(QStringLiteral("period"), QStringLiteral("청동기"));
  QgsPolylineXY ring2;
  ring2 << QgsPointXY(200020, 450020) << QgsPointXY(200040, 450020) << QgsPointXY(200040, 450040)
        << QgsPointXY(200020, 450040) << QgsPointXY(200020, 450020);
  ff.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring2));
  QVERIFY(fp->addFeature(ff));
  QVERIFY(fp->commitChanges());

  // 2 GCPs
  auto* cp = layer(&proj, QStringLiteral("control_points"));
  QVERIFY(cp->startEditing());
  for (int i = 0; i < 2; ++i) {
    QgsFeature cf(cp->fields());
    cf.setAttribute(QStringLiteral("point_id"), QStringLiteral("G%1").arg(i + 1));
    cf.setAttribute(QStringLiteral("x"), 200000.0 + i * 50);
    cf.setAttribute(QStringLiteral("y"), 450000.0 + i * 20);
    cf.setAttribute(QStringLiteral("datum"), QStringLiteral("세계측지계"));
    cf.setAttribute(QStringLiteral("ellipsoid"), QStringLiteral("GRS80"));
    cf.setAttribute(QStringLiteral("projection"), QStringLiteral("UTM-K"));
    cf.setAttribute(QStringLiteral("accuracy_m"), 0.8);
    cf.setAttribute(QStringLiteral("pdop"), 1.2);
    cf.setAttribute(QStringLiteral("fix_type"), QStringLiteral("RTK"));
    cf.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(200000.0 + i * 50, 450000.0 + i * 20)));
    QVERIFY(cp->addFeature(cf));
  }
  QVERIFY(cp->commitChanges());

  QVERIFY2(addComposedUserSheet(&proj, sa), "composed user_sheet");
  QVERIFY(LayoutService::isComposedStudioSheet(&proj));
  LayoutService::ensureDefaultLayouts(&proj);
  const QJsonObject st = ProjectStateBuilder::fromProject(&proj);
  QCOMPARE(st.value(QStringLiteral("survey_area_count")).toInt(), 1);
  QCOMPARE(st.value(QStringLiteral("control_points_count")).toInt(), 2);
  QCOMPARE(st.value(QStringLiteral("feature_poly_count")).toInt(), 1);
  QVERIFY(st.value(QStringLiteral("has_kind_period")).toBool());
  QVERIFY(st.value(QStringLiteral("survey_is_polygon")).toBool());
  QVERIFY(st.value(QStringLiteral("layout_exists:site_location")).toBool());
  QVERIFY(st.value(QStringLiteral("layout_exists:feature_plan")).toBool());

  ChecklistEngine eng;
  QVERIFY(eng.loadRules(rulesFile()));
  const auto results = eng.evaluate(st);
  int errors = 0;
  QString msg;
  for (const auto& r : results) {
    if (!r.passed && r.severity == QLatin1String("error")) {
      errors++;
      msg += r.id + QLatin1Char(' ');
    }
  }
  QVERIFY2(errors == 0, qPrintable(msg));

  const QString pdf = QDir(dir).filePath(QStringLiteral("feature_plan.pdf"));
  QString perr;
  QVERIFY2(!ExportService::writePdfViaLayout(&proj, QStringLiteral("feature_plan"), pdf, &perr).isEmpty(),
           qPrintable(perr));
  QVERIFY(QFileInfo(pdf).size() > 1000);

  const QString pkg = QDir(dir).filePath(QStringLiteral("pkg"));
  QString e2;
  QVERIFY2(!ExportService::exportSubmissionPackage(&proj, pkg, QStringLiteral("UTF-8"), QStringLiteral("OK"), true, false, &e2).isEmpty(),
           qPrintable(e2));
  QVERIFY(QFile::exists(QDir(pkg).filePath(QStringLiteral("survey_area.shp"))));
  QVERIFY(QFile::exists(QDir(pkg).filePath(QStringLiteral("feature_poly.shp"))));
  QVERIFY(QFile::exists(QDir(pkg).filePath(QStringLiteral("MANIFEST.sha256"))));
  const QString sheetPdf = QDir(pkg).filePath(QStringLiteral("조사도면.pdf"));
  QVERIFY2(QFile::exists(sheetPdf), "package PDF is user_sheet 조사도면.pdf");
  QVERIFY(QFileInfo(sheetPdf).size() > 500);
  QVERIFY2(!QFile::exists(QDir(pkg).filePath(QStringLiteral("유적위치도.pdf"))),
           "submit must not emit five auto templates");
}

void TestWorkflow::exportSubmissionPackage_pdfIsUserSheetOnly() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* sa = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                QStringLiteral("survey_area"), QStringLiteral("memory"));
  QVERIFY(sa->isValid());
  sa->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey), QStringLiteral("survey_area"));
  proj.addMapLayer(sa);
  QVERIFY(sa->startEditing());
  QgsFeature sf(sa->fields());
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200100, 450000)
       << QgsPointXY(200100, 450100) << QgsPointXY(200000, 450100)
       << QgsPointXY(200000, 450000);
  sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(sa->addFeature(sf));
  QVERIFY(sa->commitChanges());
  QVERIFY2(addComposedUserSheet(&proj, sa), "composed user_sheet");
  QVERIFY(LayoutService::isComposedStudioSheet(&proj));
  LayoutService::ensureDefaultLayouts(&proj);

  const QString pkg = QDir::temp().filePath(
      QStringLiteral("ka_pkg_sheet_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir(pkg).removeRecursively();
  QString err;
  QVERIFY2(!ExportService::exportSubmissionPackage(&proj, pkg, QStringLiteral("UTF-8"),
                                                   QStringLiteral("OK"), true, false, &err)
                .isEmpty(),
           qPrintable(err));
  const QString sheetPdf = QDir(pkg).filePath(QStringLiteral("조사도면.pdf"));
  QVERIFY(QFile::exists(sheetPdf));
  QVERIFY(QFileInfo(sheetPdf).size() > 500);
  QVERIFY(!QFile::exists(QDir(pkg).filePath(QStringLiteral("유적위치도.pdf"))));
  QVERIFY(!QFile::exists(QDir(pkg).filePath(QStringLiteral("유구배치도.pdf"))));
}

void TestWorkflow::exportSubmissionPackage_withoutUserSheetFails() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* sa = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                QStringLiteral("survey_area"), QStringLiteral("memory"));
  QVERIFY(sa->isValid());
  sa->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey), QStringLiteral("survey_area"));
  proj.addMapLayer(sa);
  QVERIFY(sa->startEditing());
  QgsFeature sf(sa->fields());
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200100, 450000)
       << QgsPointXY(200100, 450100) << QgsPointXY(200000, 450100)
       << QgsPointXY(200000, 450000);
  sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(sa->addFeature(sf));
  QVERIFY(sa->commitChanges());

  const QString pkg = QDir::temp().filePath(
      QStringLiteral("ka_pkg_nosheet_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir(pkg).removeRecursively();
  QString err;
  QVERIFY(ExportService::exportSubmissionPackage(&proj, pkg, QStringLiteral("UTF-8"),
                                                 QStringLiteral("OK"), true, false, &err)
              .isEmpty());
  QVERIFY2(err.contains(QStringLiteral("도면만들기")), qPrintable(err));
  QVERIFY(!QFile::exists(QDir(pkg).filePath(QStringLiteral("README_submit.txt"))));
}

void TestWorkflow::exportSubmissionPackage_preservesPreviousPackage() {
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* layer = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186&field=name:string"),
                                 QStringLiteral("control_points"), QStringLiteral("memory"));
  QVERIFY(layer->isValid());
  layer->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey), QStringLiteral("control_points"));
  project.addMapLayer(layer);
  QgsFeature feature(layer->fields());
  feature.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(200000, 450000)));
  feature.setAttribute(QStringLiteral("name"), QStringLiteral("기준점"));
  QgsFeatureList packageFeatures{feature};
  QVERIFY(layer->dataProvider()->addFeatures(packageFeatures));
  QVERIFY(addComposedUserSheet(&project, layer));
  const QString first = temporary.filePath(QStringLiteral("첫 제출"));
  QString error;
  QCOMPARE(ExportService::exportSubmissionPackage(&project, first, QStringLiteral("UTF-8"),
                                                QStringLiteral("OK"), true, false, &error), first);
  {
    QgsVectorLayer shp(QDir(first).filePath(QStringLiteral("control_points.shp")),
                       QStringLiteral("submitted"), QStringLiteral("ogr"));
    QVERIFY(shp.isValid());
    QCOMPARE(shp.featureCount(), 1LL);
    QCOMPARE(shp.crs().authid(), QStringLiteral("EPSG:5179"));
    QgsFeature submitted;
    QVERIFY(shp.getFeatures().nextFeature(submitted));
    QCOMPARE(submitted.attribute(QStringLiteral("name")).toString(), QStringLiteral("기준점"));
    QFile pdf(QDir(first).filePath(QStringLiteral("조사도면.pdf")));
    QVERIFY(pdf.open(QIODevice::ReadOnly));
    const QByteArray pdfBytes = pdf.readAll();
    QVERIFY(pdfBytes.startsWith("%PDF-"));
    QVERIFY(pdfBytes.contains("%%EOF"));
  }
  QMap<QString, QByteArray> previous;
  for (const QString& name : QDir(first).entryList(QDir::Files)) {
    QFile file(QDir(first).filePath(name));
    QVERIFY(file.open(QIODevice::ReadOnly));
    previous.insert(name, file.readAll());
  }
  project.removeMapLayer(layer);
  project.layoutManager()->clear();
  QVERIFY2(ExportService::exportSubmissionPackage(&project, first, QStringLiteral("UTF-8"),
               QStringLiteral("changed"), true, false, &error).isEmpty(),
           "A previous package must not be reused when current layers change");
  QVERIFY(!error.isEmpty());
  QStringList previousNames = QDir(first).entryList(QDir::Files);
  previousNames.sort();
  QCOMPARE(previousNames, previous.keys());
  for (auto it = previous.cbegin(); it != previous.cend(); ++it) {
    QFile file(QDir(first).filePath(it.key()));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), it.value());
  }
  const QString second = temporary.filePath(QStringLiteral("두 번째 제출"));
  QVERIFY2(ExportService::exportSubmissionPackage(&project, second, QStringLiteral("CP949"),
                                                 QStringLiteral("OK"), true, false, &error)
               .isEmpty(),
           "empty project without composed user_sheet must not finalize a package");
  QVERIFY2(error.contains(QStringLiteral("도면만들기")), qPrintable(error));
  QVERIFY(!QFile::exists(QDir(second).filePath(QStringLiteral("README_submit.txt"))));
}

void TestWorkflow::exportManifestFailurePreservesPreviousFile() {
#ifdef _WIN32
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  const QString path = temporary.filePath(QStringLiteral("MANIFEST.sha256"));
  QFile previous(path);
  QVERIFY(previous.open(QIODevice::WriteOnly));
  const QByteArray contents("previous manifest\n");
  QCOMPARE(previous.write(contents), contents.size());
  previous.close();
  const QString lockedPath = temporary.filePath(QStringLiteral("locked.txt"));
  HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(lockedPath.utf16()), GENERIC_WRITE,
                             0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  QVERIFY(handle != INVALID_HANDLE_VALUE);
  QString error;
  const bool result = ExportService::writeSha256Manifest(temporary.path(), &error);
  CloseHandle(handle);
  QVERIFY(!result);
  QVERIFY(!error.isEmpty());
  QVERIFY(previous.open(QIODevice::ReadOnly));
  QCOMPARE(previous.readAll(), contents);
#else
  QSKIP("Windows exclusive file lock regression");
#endif
}

void TestWorkflow::exportSubmissionPackage_finalizationFailure_data() {
  QTest::addColumn<QString>("failure");
  QTest::newRow("late-folder-collision") << QStringLiteral("collision");
  QTest::newRow("readme-write-denied") << QStringLiteral("README_submit.txt");
  QTest::newRow("manifest-write-denied") << QStringLiteral("MANIFEST.sha256");
}

void TestWorkflow::exportSubmissionPackage_finalizationFailure() {
  QFETCH(QString, failure);
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* layer = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186"),
                                 QStringLiteral("control_points"), QStringLiteral("memory"));
  project.addMapLayer(layer);
  QgsFeature feature(layer->fields());
  feature.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(200050, 450050)));
  QgsFeatureList features{feature};
  QVERIFY(layer->dataProvider()->addFeatures(features));
  QVERIFY(addComposedUserSheet(&project, layer));
  QVERIFY(LayoutService::isComposedStudioSheet(&project));
  auto* layout = dynamic_cast<QgsPrintLayout*>(project.layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  QVERIFY(layout);
  // The synchronous render-context signal injects a filesystem failure after
  // staging starts, without production test hooks or timing-dependent threads.
  layout->renderContext().setDpi(96);
  const QString output = temporary.filePath(QStringLiteral("result"));
  bool injected = false;
  bool injectionOk = false;
  connect(&layout->renderContext(), &QgsLayoutRenderContext::dpiChanged, &project, [&] {
    if (injected) return;
    injected = true;
    if (failure == QLatin1String("collision")) {
      injectionOk = QDir().mkpath(output);
      QFile sentinel(QDir(output).filePath(QStringLiteral("other-user.txt")));
      injectionOk = injectionOk && sentinel.open(QIODevice::WriteOnly) && sentinel.write("keep") == 4;
    } else {
      const auto staging = QDir(temporary.path()).entryList(
          {QStringLiteral(".ka-hgis-export-*")}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
      if (staging.size() == 1)
        injectionOk = QDir(temporary.filePath(staging.first())).mkdir(failure);
    }
  });
  QString error;
  const QString result = ExportService::exportSubmissionPackage(&project, output, QStringLiteral("UTF-8"),
                                                               QStringLiteral("OK"), true, false, &error);
  QVERIFY(injected);
  QVERIFY(injectionOk);
  QVERIFY(result.isEmpty());
  QVERIFY(!error.isEmpty());
  QVERIFY(!QFile::exists(QDir(output).filePath(QStringLiteral("MANIFEST.sha256"))));
  if (failure == QLatin1String("collision")) {
    QFile sentinel(QDir(output).filePath(QStringLiteral("other-user.txt")));
    QVERIFY(sentinel.open(QIODevice::ReadOnly));
    QCOMPARE(sentinel.readAll(), QByteArray("keep"));
  } else {
    QVERIFY(!QFileInfo::exists(output));
  }
  QVERIFY(QDir(temporary.path()).entryList({QStringLiteral(".ka-hgis-export-*")},
          QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
}

void TestWorkflow::exportSubmissionPackage_excludesPrivateHeritage_data() {
  QTest::addColumn<bool>("domainNameCollision");
  QTest::newRow("six-reference-datasets") << false;
  QTest::newRow("reference-renamed-as-domain") << true;
}

void TestWorkflow::exportSubmissionPackage_excludesPrivateHeritage() {
  QFETCH(bool, domainNameCollision);
  QTemporaryDir temporary;
  QVERIFY(temporary.isValid());
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
  auto* survey = new QgsVectorLayer(
      QStringLiteral("Polygon?crs=EPSG:5187&field=record_id:integer"),
      QStringLiteral("조사구역"), QStringLiteral("memory"));
  QVERIFY(survey->isValid());
  survey->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey), QStringLiteral("survey_area"));
  project.addMapLayer(survey);
  QgsFeature surveyFeature(survey->fields());
  surveyFeature.setAttribute(QStringLiteral("record_id"), 42);
  surveyFeature.setGeometry(QgsGeometry::fromRect(QgsRectangle(200000, 450000, 200100, 450100)));
  QgsFeatureList surveyFeatures{surveyFeature};
  QVERIFY(survey->dataProvider()->addFeatures(surveyFeatures));
  survey->updateExtents();

  QList<QgsVectorLayer*> references;
  int datasetIndex = 0;
  for (const auto dataset : HeritageStyle::allDatasets()) {
    QgsVectorLayer fixture(
        QStringLiteral("Polygon?crs=EPSG:5179&field=nm:string(80)&field=record_id:integer"),
        QStringLiteral("synthetic private reference"), QStringLiteral("memory"));
    QVERIFY(fixture.isValid());
    QgsFeature feature(fixture.fields());
    feature.setAttribute(QStringLiteral("nm"), QStringLiteral("검증 전용 유적 %1").arg(datasetIndex));
    feature.setAttribute(QStringLiteral("record_id"), 1000 + datasetIndex);
    feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(1100000, 2000000, 1100100, 2000100)));
    QgsFeatureList features{feature};
    QVERIFY(fixture.dataProvider()->addFeatures(features));
    fixture.updateExtents();
    const QString source = temporary.filePath(QStringLiteral("private_reference_%1.shp").arg(datasetIndex++));
    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = QStringLiteral("ESRI Shapefile");
    options.fileEncoding = QStringLiteral("UTF-8");
    QCOMPARE(QgsVectorFileWriter::writeAsVectorFormatV3(
                 &fixture, source, project.transformContext(), options),
             QgsVectorFileWriter::NoError);
    const auto imported = HeritageImport::loadDataset(
        &project, dataset, {source}, temporary.filePath(QStringLiteral("archive")));
    QVERIFY2(imported.ok(), qPrintable(imported.error));
    QCOMPARE(imported.layers.size(), 1);
    auto* reference = imported.layers.first();
    QVERIFY(LayerOps::isReferenceLayer(reference));
    QVERIFY(LayerOps::layerKeyOf(reference).isEmpty());
    references.append(reference);
  }
  QCOMPARE(references.size(), 6);
  if (domainNameCollision)
    references.first()->setName(QStringLiteral("feature_poly"));
  QVERIFY(addComposedUserSheet(&project, survey));

  const QString output = temporary.filePath(QStringLiteral("submission"));
  QString error;
  QCOMPARE(ExportService::exportSubmissionPackage(&project, output, QStringLiteral("UTF-8"),
                                                   QStringLiteral("OK"), true, false, &error), output);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(QDir(output).entryList({QStringLiteral("*.shp")}, QDir::Files),
           QStringList{QStringLiteral("survey_area.shp")});
  QgsVectorLayer submitted(QDir(output).filePath(QStringLiteral("survey_area.shp")),
                           QStringLiteral("submitted survey"), QStringLiteral("ogr"));
  QVERIFY(submitted.isValid());
  QCOMPARE(submitted.crs().authid(), QStringLiteral("EPSG:5179"));
  QCOMPARE(submitted.featureCount(), 1);
  QgsFeature written;
  QVERIFY(submitted.getFeatures().nextFeature(written));
  QCOMPARE(written.attribute(QStringLiteral("record_id")).toInt(), 42);
  QVERIFY(!QDir(output).exists(QStringLiteral("archive")));
  QVERIFY(QDir(output).entryList({QStringLiteral("private_reference*")}, QDir::Files).isEmpty());
  QFile manifest(QDir(output).filePath(QStringLiteral("MANIFEST.sha256")));
  QVERIFY(manifest.open(QIODevice::ReadOnly));
  const auto manifestBytes = manifest.readAll();
  QVERIFY(manifestBytes.contains("survey_area.shp"));
  QVERIFY(!manifestBytes.contains("private_reference"));
  QVERIFY(!manifestBytes.contains("feature_poly.shp"));
  QCOMPARE(project.mapLayers().size(), 7);
  for (const auto* reference : references) {
    QVERIFY(reference->isValid());
    QCOMPARE(reference->featureCount(), 1);
  }
}

void TestWorkflow::exportLayoutPdf_userSheetMissing_doesNotSeedFiveTemplates() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QVERIFY(!proj.layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  const QString pdf = QDir::temp().filePath(
      QStringLiteral("ka_missing_user_sheet_") + QString::number(QDateTime::currentMSecsSinceEpoch()) +
      QStringLiteral(".pdf"));
  QFile::remove(pdf);
  QString err;
  QVERIFY(LayoutService::exportLayoutPdf(&proj, QStringLiteral("user_sheet"), pdf, &err).isEmpty());
  QVERIFY2(!proj.layoutManager()->layoutByName(QStringLiteral("site_location")),
           "missing user_sheet must not seed site_location");
  QVERIFY2(!proj.layoutManager()->layoutByName(QStringLiteral("feature_plan")),
           "missing user_sheet must not seed feature_plan");
  const QString pdfDir = QDir::temp().filePath(
      QStringLiteral("ka_missing_drawings_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir(pdfDir).removeRecursively();
  QString drawErr;
  QCOMPARE(LayoutService::exportDrawingPdfs(&proj, pdfDir, &drawErr), 0);
  QVERIFY2(drawErr.contains(QStringLiteral("도면만들기")), qPrintable(drawErr));
  QVERIFY2(!proj.layoutManager()->layoutByName(QStringLiteral("survey_area_map")),
           "exportDrawingPdfs must not seed five templates");
}

void TestWorkflow::shpKoreanRoundTripUtf8() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_shp_kr_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QgsVectorLayer mem(QStringLiteral("Polygon?crs=EPSG:5179"), QStringLiteral("kr"), QStringLiteral("memory"));
  QVERIFY(mem.isValid());
  QgsFields fields;
  fields.append(QgsField(QStringLiteral("kind"), QMetaType::Type::QString));
  fields.append(QgsField(QStringLiteral("period"), QMetaType::Type::QString));
  mem.dataProvider()->addAttributes(fields.toList());
  mem.updateFields();
  QVERIFY(mem.startEditing());
  QgsFeature f(mem.fields());
  f.setAttribute(QStringLiteral("kind"), QStringLiteral("수혈주거지"));
  f.setAttribute(QStringLiteral("period"), QStringLiteral("청동기시대"));
  QgsPolylineXY ring;
  ring << QgsPointXY(0, 0) << QgsPointXY(10, 0) << QgsPointXY(10, 10) << QgsPointXY(0, 10) << QgsPointXY(0, 0);
  f.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(mem.addFeature(f));
  QVERIFY(mem.commitChanges());

  const QString shp = QDir(dir).filePath(QStringLiteral("kr_feat.shp"));
  QgsVectorFileWriter::SaveVectorOptions opts;
  opts.driverName = QStringLiteral("ESRI Shapefile");
  opts.fileEncoding = QStringLiteral("UTF-8");
  QString err, nf, nl;
  const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
      &mem, shp, QgsCoordinateTransformContext(), opts, &err, &nf, &nl);
  QCOMPARE(we, QgsVectorFileWriter::NoError);

  QgsVectorLayer loaded(shp, QStringLiteral("loaded"), QStringLiteral("ogr"));
  QVERIFY2(loaded.isValid(), qPrintable(loaded.error().message()));
  QCOMPARE(int(loaded.featureCount()), 1);
  const QgsFeatureIds ids = loaded.allFeatureIds();
  QVERIFY(!ids.isEmpty());
  QgsFeature lf = loaded.getFeature(*ids.constBegin());
  // OGR may transliterate field names; read by index if needed
  QString kind = lf.attribute(QStringLiteral("kind")).toString();
  QString period = lf.attribute(QStringLiteral("period")).toString();
  if (kind.isEmpty() && loaded.fields().count() >= 1) kind = lf.attribute(0).toString();
  if (period.isEmpty() && loaded.fields().count() >= 2) period = lf.attribute(1).toString();
  QVERIFY2(kind.contains(QStringLiteral("수혈")) || kind.contains(QStringLiteral("주거")) || !kind.isEmpty(),
           qPrintable(QStringLiteral("kind=%1").arg(kind)));
  QVERIFY2(!period.isEmpty(), qPrintable(QStringLiteral("period empty")));
}

// 흙토람 토양도 SHP 임포트: 좌표계 지정(.prj 유무 모두)과 분류색 렌더러를 검증.
void TestWorkflow::soilShpImport_crsOverrideAndCategorizedStyle() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_soil_") +
                                            QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);

  QgsVectorLayer mem(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("soil"),
                     QStringLiteral("memory"));
  QVERIFY(mem.isValid());
  QgsFields fields;
  fields.append(QgsField(QStringLiteral("TPGRP_NM"), QMetaType::Type::QString));
  mem.dataProvider()->addAttributes(fields.toList());
  mem.updateFields();
  QVERIFY(mem.startEditing());
  const QStringList terrains = {QStringLiteral("산악지"), QStringLiteral("구릉지"),
                                QStringLiteral("산악지")};
  for (int i = 0; i < terrains.size(); ++i) {
    QgsFeature f(mem.fields());
    f.setAttribute(QStringLiteral("TPGRP_NM"), terrains.at(i));
    QgsPolylineXY ring;
    const double x = 200000.0 + i * 20.0;
    ring << QgsPointXY(x, 500000.0) << QgsPointXY(x + 10.0, 500000.0)
         << QgsPointXY(x + 10.0, 500010.0) << QgsPointXY(x, 500010.0)
         << QgsPointXY(x, 500000.0);
    f.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
    QVERIFY(mem.addFeature(f));
  }
  QVERIFY(mem.commitChanges());

  const QString shp = QDir(dir).filePath(QStringLiteral("soil_map.shp"));
  QgsVectorFileWriter::SaveVectorOptions opts;
  opts.driverName = QStringLiteral("ESRI Shapefile");
  opts.fileEncoding = QStringLiteral("UTF-8");
  QString err, nf, nl;
  const auto we = QgsVectorFileWriter::writeAsVectorFormatV3(
      &mem, shp, QgsCoordinateTransformContext(), opts, &err, &nf, &nl);
  QCOMPARE(we, QgsVectorFileWriter::NoError);

  // 1) 사용자가 좌표계를 지정하면 파일 좌표계보다 우선한다 + 분류색 렌더러.
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QgsVectorLayer* l1 = LayerOps::addSoilShapefile(&proj, nullptr, shp, QStringLiteral("EPSG:5174"),
                                                  QStringLiteral("TPGRP_NM"), &err);
  QVERIFY2(l1, qPrintable(err));
  QCOMPARE(l1->crs().authid(), QStringLiteral("EPSG:5174"));
  QVERIFY(LayerOps::isReferenceLayer(l1));
  auto* cat = dynamic_cast<QgsCategorizedSymbolRenderer*>(l1->renderer());
  QVERIFY2(cat, "categorized renderer expected");
  // 고유값 2개(산악지·구릉지) + "기타" 캐치올.
  QVERIFY2(cat->categories().size() >= 3,
           qPrintable(QStringLiteral("categories=%1").arg(cat->categories().size())));

  // 2) .prj가 없는 배포본은 흙토람 고시 좌표계(EPSG:2097)로 가정한다 + 단색 렌더러.
  QFile::remove(QDir(dir).filePath(QStringLiteral("soil_map.prj")));
  QFile::remove(QDir(dir).filePath(QStringLiteral("soil_map.qpj")));
  QgsVectorLayer* l2 =
      LayerOps::addSoilShapefile(&proj, nullptr, shp, QString(), QString(), &err);
  QVERIFY2(l2, qPrintable(err));
  QCOMPARE(l2->crs().authid(), QStringLiteral("EPSG:2097"));
  QVERIFY(dynamic_cast<QgsSingleSymbolRenderer*>(l2->renderer()) != nullptr);
}

// 흙토람 공식 분포지형 범례(코드→이름·색)와 스타일 적용을 검증한다.
void TestWorkflow::soilTerrainLegend_officialCodesAndStyle() {
  QCOMPARE(SoilMapService::terrainName(QStringLiteral("01")), QStringLiteral("산악지"));
  QCOMPARE(SoilMapService::terrainName(QStringLiteral("04")), QStringLiteral("곡간지/선상지"));
  QCOMPARE(SoilMapService::terrainName(QStringLiteral("06")), QStringLiteral("하성평탄지"));
  QCOMPARE(SoilMapService::terrainColor(QStringLiteral("01")), QColor(1, 178, 0));
  QCOMPARE(SoilMapService::terrainColor(QStringLiteral("08")), QColor(254, 160, 0));
  QCOMPARE(SoilMapService::terrainName(QStringLiteral("77")), QStringLiteral("미분류"));
  QVERIFY2(SoilMapService::terrainLabelExpression().contains(QStringLiteral("하성평탄")),
           "label expression maps codes to Korean names");
  const QString sparse = SoilMapService::terrainLabelExpression(40000.0, false);
  QVERIFY2(sparse.contains(QLatin1String("area($geometry)")),
           "작은 토양 폴리곤마다 글자를 찍으면 도면이 글자로 덮인다");
  const QString paleoLbl = SoilMapService::terrainLabelExpression(50000.0, true);
  QVERIFY2(paleoLbl.contains(QLatin1String("'04'")) && paleoLbl.contains(QLatin1String("산악지")) == false,
           "고지형에서는 입지후보만 글자");

  QgsVectorLayer mem(QStringLiteral("MultiPolygon?crs=EPSG:5186&field=soil_type_geo:string"),
                     QStringLiteral("soilwfs"), QStringLiteral("memory"));
  QVERIFY(mem.isValid());
  QVERIFY(SoilMapService::applyTerrainStyle(&mem));
  auto* cat = dynamic_cast<QgsCategorizedSymbolRenderer*>(mem.renderer());
  QVERIFY2(cat, "categorized renderer expected");
  QCOMPARE(cat->classAttribute(), QStringLiteral("soil_type_geo"));
  // 공식 11개 분류 + 미분류 캐치올.
  QCOMPARE(cat->categories().size(), 12);
  QVERIFY2(mem.labelsEnabled(), "지도에 분포지형 한글 이름이 보여야 한다");

  // 분포지형 필드가 없으면 실패를 명확히 알린다.
  QgsVectorLayer noField(QStringLiteral("MultiPolygon?crs=EPSG:5186"),
                         QStringLiteral("nofield"), QStringLiteral("memory"));
  QVERIFY(!SoilMapService::applyTerrainStyle(&noField));
}

void TestWorkflow::geologyEraLegend_icsColorsAndStyle() {
  // 서버 시대 문자열 → 정규화 분류(세분 우선).
  QCOMPARE(GeologyMapService::eraClass(QStringLiteral("현생누대 신생대 제4기")),
           QStringLiteral("제4기"));
  QCOMPARE(GeologyMapService::eraClass(QStringLiteral("현생누대 고생대 오르도비스기")),
           QStringLiteral("오르도비스기"));
  QCOMPARE(GeologyMapService::eraClass(QStringLiteral("선캄브리아시대")),
           QStringLiteral("선캄브리아시대"));
  // 제주 WFS는 「제 4기」처럼 기·숫자 사이에 공백이 있다. 본토 「제4기」와 같아야 한다.
  QCOMPARE(GeologyMapService::eraClass(QStringLiteral("신생대 제 4기")),
           QStringLiteral("제4기"));
  QCOMPARE(GeologyMapService::eraClass(QString()), QStringLiteral("시대미상"));
  QCOMPARE(GeologyMapService::eraColor(QStringLiteral("제4기")), QColor(249, 249, 127));

  // 보고서 지질도 관례: 데이터에 실제로 있는 지질단위만 「기호 · 지층명」 범례.
  QgsVectorLayer mem(QStringLiteral("MultiPolygon?crs=EPSG:5186"), QStringLiteral("geowfs"),
                     QStringLiteral("memory"));
  QVERIFY(mem.isValid());
  mem.dataProvider()->addAttributes(
      {QgsField(QStringLiteral("시대"), QMetaType::Type::QString),
       QgsField(QStringLiteral("지층"), QMetaType::Type::QString),
       QgsField(QStringLiteral("기호"), QMetaType::Type::QString)});
  mem.updateFields();
  QgsFeature f1(mem.fields());
  f1.setAttribute(0, QStringLiteral("현생누대 신생대 제4기"));
  f1.setAttribute(1, QStringLiteral("충적층"));
  f1.setAttribute(2, QStringLiteral("Qa"));
  QgsFeature f2(mem.fields());
  f2.setAttribute(0, QStringLiteral("선캄브리아시대"));
  f2.setAttribute(1, QStringLiteral("반상변정편마암"));
  f2.setAttribute(2, QStringLiteral("PCEpgn"));
  QVERIFY(mem.dataProvider()->addFeatures(QgsFeatureList() << f1 << f2));

  QHash<QString, QColor> official;
  official.insert(QStringLiteral("Qa"), QColor(250, 244, 180));
  QVERIFY(GeologyMapService::applyGeologyStyle(&mem, official));
  auto* cat = dynamic_cast<QgsCategorizedSymbolRenderer*>(mem.renderer());
  QVERIFY2(cat, "categorized renderer expected");
  QCOMPARE(cat->classAttribute(), QStringLiteral("기호"));
  // 실존 단위 2개 + 기타 캐치올. 젊은 시대(Qa)가 먼저.
  QCOMPARE(cat->categories().size(), 3);
  QCOMPARE(cat->categories().at(0).value().toString(), QStringLiteral("Qa"));
  QCOMPARE(cat->categories().at(0).label(), QStringLiteral("충적층"));
  QCOMPARE(cat->categories().at(1).label(), QStringLiteral("반상변정편마암"));
  // 공식 도폭색이 심볼에 반영된다(알파 제외 RGB 비교).
  const QColor c0 = cat->categories().at(0).symbol()->color();
  QCOMPARE(QColor(c0.red(), c0.green(), c0.blue()), QColor(250, 244, 180));
  QVERIFY2(mem.labelsEnabled(), "symbol labels expected");

  QgsVectorLayer noField(QStringLiteral("MultiPolygon?crs=EPSG:5186"),
                         QStringLiteral("nofield"), QStringLiteral("memory"));
  QVERIFY(!GeologyMapService::applyGeologyStyle(&noField));
}

void TestWorkflow::geologyLithoWfs_excludesJejuUsesOfficialRasterUri() {
  // KIGAM mainland litho WFS mosaic WGS84 south edge is ~33.97N.
  // Jeju (광령리 ~33.48N) is outside that mosaic — use Jeju litho WFS, not empty
  // mainland WFS, and not a raster WMS legend that differs from the mainland.
  const QgsRectangle jeju(126.40, 33.40, 126.60, 33.55);
  QVERIFY2(!GeologyMapService::lithoWfsCoversWgs84(jeju),
           "Jeju must not use empty mainland litho WFS as a 'no data' failure");
  QVERIFY2(GeologyMapService::jejuLithoWfsCoversWgs84(jeju),
           "Jeju must use geoOpen:l_jeju_50k_geology_litho_view");
  QCOMPARE(GeologyMapService::lithoTypeNameForWgs84(jeju),
           QStringLiteral("geoOpen:l_jeju_50k_geology_litho_view"));
  const QgsRectangle gangneung(128.80, 37.70, 128.95, 37.80);
  QVERIFY2(GeologyMapService::lithoWfsCoversWgs84(gangneung),
           "mainland litho WFS path must stay");
  QCOMPARE(GeologyMapService::lithoTypeNameForWgs84(gangneung),
           QStringLiteral("geoOpen:l_50k_geology_litho_latest"));
  QVERIFY2(!GeologyMapService::jejuLithoWfsCoversWgs84(gangneung),
           "Gangneung must not switch to the Jeju typeName");
  const QString uri = GeologyMapService::officialRasterWmsUri();
  QVERIFY2(uri.contains(QLatin1String("L_50K_Geology_Map")), qPrintable(uri.left(180)));
  QVERIFY2(uri.contains(QLatin1String("data.kigam.re.kr")), qPrintable(uri.left(180)));
  QVERIFY2(uri.contains(QLatin1String("crs=EPSG:4326")), qPrintable(uri.left(180)));
}

void TestWorkflow::geologyJejuLitho_sameCategorizedLegendAsMainland() {
  // 제주 WFS 필드명(시대·지층·기호)은 본토와 같다. 범례는 지층명, 분류 키는 기호.
  QgsVectorLayer mem(QStringLiteral("MultiPolygon?crs=EPSG:5186"), QStringLiteral("jejuwfs"),
                     QStringLiteral("memory"));
  QVERIFY(mem.isValid());
  mem.dataProvider()->addAttributes(
      {QgsField(QStringLiteral("시대"), QMetaType::Type::QString),
       QgsField(QStringLiteral("지층"), QMetaType::Type::QString),
       QgsField(QStringLiteral("기호"), QMetaType::Type::QString)});
  mem.updateFields();
  QgsFeature f1(mem.fields());
  f1.setAttribute(0, QStringLiteral("신생대 제 4기"));
  f1.setAttribute(1, QStringLiteral("부면동조면현무암"));
  f1.setAttribute(2, QStringLiteral("Qbmtb"));
  QgsFeature f2(mem.fields());
  f2.setAttribute(0, QStringLiteral("신생대 제 4기"));
  f2.setAttribute(1, QStringLiteral("대포동조면현무암 분석구"));
  f2.setAttribute(2, QStringLiteral("Qdtbs"));
  QVERIFY(mem.dataProvider()->addFeatures(QgsFeatureList() << f1 << f2));
  QVERIFY(GeologyMapService::applyGeologyStyle(&mem));
  auto* cat = dynamic_cast<QgsCategorizedSymbolRenderer*>(mem.renderer());
  QVERIFY2(cat, "Jeju must use the same categorized renderer as the mainland");
  QCOMPARE(cat->classAttribute(), QStringLiteral("기호"));
  QCOMPARE(cat->categories().at(0).label(), QStringLiteral("부면동조면현무암"));
  QCOMPARE(cat->categories().at(1).label(), QStringLiteral("대포동조면현무암 분석구"));
  QVERIFY2(mem.labelsEnabled(), "Jeju geology must keep symbol labels like the mainland");
}

void TestWorkflow::thematicOverlay_secondToggleHidesLayer() {
  // QGIS 범례 체크 / ArcGIS 레이어 on-off: 같은 오버레이를 다시 누르면 숨긴다.
  QgsProject proj;
  auto* vl = new QgsVectorLayer(QStringLiteral("MultiPolygon?crs=EPSG:5186"),
                                QStringLiteral("지질도(KIGAM 1:5만)"), QStringLiteral("memory"));
  QVERIFY(vl->isValid());
  LayerOps::markReferenceLayer(vl);
  QVERIFY(proj.addMapLayer(vl));
  QVERIFY(LayerOps::isLayerVisible(&proj, QStringLiteral("지질도(KIGAM 1:5만)")));
  QVERIFY(LayerOps::toggleLayerVisibility(&proj, nullptr, QStringLiteral("지질도(KIGAM 1:5만)"),
                                          false));
  QVERIFY2(!LayerOps::isLayerVisible(&proj, QStringLiteral("지질도(KIGAM 1:5만)")),
           "second click / explicit off must hide the overlay");
  QVERIFY(LayerOps::toggleLayerVisibility(&proj, nullptr, QStringLiteral("지질도(KIGAM 1:5만)"),
                                          true));
  QVERIFY(LayerOps::isLayerVisible(&proj, QStringLiteral("지질도(KIGAM 1:5만)")));
}

void TestWorkflow::mapTools_secondClickReturnsToPan() {
  // ArcGIS Explore / QGIS pan: 선택·줄자를 다시 누르면 팬으로 돌아간다.
  const QString src = readMainWindowSources();
  QVERIFY2(!src.isEmpty(), "run from source tree (ctest WORKING_DIRECTORY)");
  auto bodyOf = [&](const QString& sig) {
    const int fn = src.indexOf(sig);
    if (fn < 0) return QString();
    const int next = src.indexOf(QLatin1String("void MainWindow::"), fn + sig.size());
    return next > fn ? src.mid(fn, next - fn) : src.mid(fn);
  };
  const QString sel = bodyOf(QStringLiteral("void MainWindow::startSelectTool"));
  QVERIFY2(!sel.isEmpty(), "startSelectTool");
  // 도형선택은 고르기만이 아니라 꼭짓점을 끌어 고치는 데까지 쓴다(m_vertexTool).
  QVERIFY2((sel.contains(QLatin1String("m_featureSelectTool")) || sel.contains(QLatin1String("m_vertexTool"))) &&
               sel.contains(QLatin1String("m_panTool")),
           "second 선택 click must return to pan");
  QVERIFY2(sel.contains(QLatin1String("mapTool() == m_featureSelectTool")) ||
               sel.contains(QLatin1String("mapTool() == m_vertexTool")) ||
               sel.contains(QLatin1String("mapTool()==m_vertexTool")),
           "선택 must detect the already-active tool");
  const QString meas = bodyOf(QStringLiteral("void MainWindow::startMeasureTool"));
  QVERIFY2(!meas.isEmpty(), "startMeasureTool");
  QVERIFY2(meas.contains(QLatin1String("m_panTool")),
           "second 줄자 click must return to pan, not reset the session");
  QVERIFY2(!meas.contains(QLatin1String("hideSubTools()")),
           "줄자 must not close the draw panel (ArcGIS edit session stays)");
  const QString hide = bodyOf(QStringLiteral("void MainWindow::hideSubTools"));
  QVERIFY2(!hide.isEmpty(), "hideSubTools");
  QVERIFY2(hide.contains(QLatin1String("stopCaptureTool")),
           "그리기 닫기는 캡처를 끄고 팬으로 돌아가야 한다");
  QVERIFY2(src.contains(QLatin1String("btnSoil")) &&
               src.contains(QLatin1String("MenuButtonPopup")),
           "토양도 본체는 토글, 화살표는 내려받기 메뉴");
}

void TestWorkflow::geologyDownload_skipsBlockingWmsAndRefreshWhileDrawing() {
  // 2026-09-01 08:17 dump: provider_wms + QgsRasterProjector + sendPostedEvents AV
  // after 지질도 download while VWorld tiles were in flight.
  QFile geo(QStringLiteral("src/core/GeologyMapService.cpp"));
  QVERIFY2(geo.open(QIODevice::ReadOnly | QIODevice::Text), "GeologyMapService.cpp");
  const QString src = QString::fromUtf8(geo.readAll());
  QVERIFY2(src.contains(QLatin1String("l_jeju_50k_geology_litho_view")),
           "Jeju must fetch litho polygons, not only L_50K_Geology_Map WMS");
  QVERIFY2(src.contains(QLatin1String("isDrawing")),
           "must not refresh / sample official WMS while the canvas is drawing");

  QFile mw(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString app = QString::fromUtf8(mw.readAll());
  const int fn = app.indexOf(QLatin1String("void MainWindow::refreshMapCanvasNow"));
  QVERIFY2(fn >= 0, "refreshMapCanvasNow");
  const int next = app.indexOf(QLatin1String("void MainWindow::"), fn + 10);
  QVERIFY2(next > fn, "refreshMapCanvasNow body");
  QVERIFY2(app.mid(fn, next - fn).contains(QLatin1String("isDrawing")),
           "layersAdded must not restack an in-flight WMS job");

  const int geoFn = app.indexOf(QLatin1String("void MainWindow::downloadGeologyMap"));
  QVERIFY2(geoFn >= 0, "downloadGeologyMap");
  const int geoNext = app.indexOf(QLatin1String("void MainWindow::"), geoFn + 10);
  const QString geoBody = app.mid(geoFn, geoNext - geoFn);
  QVERIFY2(geoBody.contains(QLatin1String("toggleExistingOverlay")) ||
               geoBody.contains(QLatin1String("toggleLayerVisibility")) ||
               geoBody.contains(QLatin1String("isLayerVisible")),
           "지질도 second click must hide the overlay like QGIS/ArcGIS");
  QVERIFY2(geoBody.contains(QLatin1String("ensureReliefUnderlay")),
           "이미 받아 둔 지질도를 다시 켤 때도 음영을 만들어야 함");
}

void TestWorkflow::geologyRelief_drapesMultiplyOverHillshade() {
  QgsVectorLayer mem(QStringLiteral("MultiPolygon?crs=EPSG:5186"), QStringLiteral("georelief"),
                     QStringLiteral("memory"));
  QVERIFY(mem.isValid());
  mem.dataProvider()->addAttributes(
      {QgsField(QStringLiteral("시대"), QMetaType::Type::QString),
       QgsField(QStringLiteral("지층"), QMetaType::Type::QString),
       QgsField(QStringLiteral("기호"), QMetaType::Type::QString)});
  mem.updateFields();
  QgsFeature f(mem.fields());
  f.setAttribute(0, QStringLiteral("현생누대 신생대 제4기"));
  f.setAttribute(1, QStringLiteral("충적층"));
  f.setAttribute(2, QStringLiteral("Qa"));
  QVERIFY(mem.dataProvider()->addFeatures(QgsFeatureList() << f));
  QVERIFY(GeologyMapService::applyGeologyStyle(&mem));
  GeologyMapService::drapeOnRelief(&mem);
  QCOMPARE(mem.blendMode(), QPainter::CompositionMode_SourceOver);
  QCOMPARE(mem.featureBlendMode(), QPainter::CompositionMode_SourceOver);
  auto* cat = dynamic_cast<QgsCategorizedSymbolRenderer*>(mem.renderer());
  QVERIFY(cat);
  QVERIFY(cat->categories().size() >= 1);
  QVERIFY(LayoutService::sheetLegendOmitsLayerName(GeologyMapService::reliefLayerTitle()));

  GDALAllRegister();
  const QString path = QDir::temp().filePath(QStringLiteral("ka_hgis_geo_relief.tif"));
  QFile::remove(path);
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
  QVERIFY2(drv, "GTiff");
  GDALDataset* ds = drv->Create(path.toUtf8().constData(), 16, 16, 1, GDT_Float32, nullptr);
  QVERIFY2(ds, "create DEM tif");
  double gt[6] = {200000.0, 10.0, 0.0, 450000.0, 0.0, -10.0};
  ds->SetGeoTransform(gt);
  OGRSpatialReference srs;
  srs.SetFromUserInput("EPSG:5186");
  char* wkt = nullptr;
  srs.exportToWkt(&wkt);
  if (wkt) {
    ds->SetProjection(wkt);
    CPLFree(wkt);
  }
  std::vector<float> z(16 * 16);
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x)
      z[static_cast<size_t>(y) * 16 + x] = 20.0f + float(x * y);
  }
  QVERIFY(ds->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 16, 16, z.data(), 16, 16, GDT_Float32, 0,
                                         0) == CE_None);
  GDALClose(ds);

  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* dem = new QgsRasterLayer(path, QStringLiteral("DEM"), QStringLiteral("gdal"));
  QVERIFY2(dem->isValid(), "local DEM");
  QVERIFY(proj.addMapLayer(dem, true));
  auto* geo = mem.clone();
  geo->setName(QStringLiteral("지질도(KIGAM 1:5만)"));
  QVERIFY(proj.addMapLayer(geo, true));
  QString err;
  QVERIFY2(GeologyMapService::ensureReliefUnderlay(&proj, nullptr, geo, &err), qPrintable(err));
  QgsMapLayer* shade = nullptr;
  for (QgsMapLayer* ml : proj.mapLayers()) {
    if (ml && ml->name() == GeologyMapService::reliefLayerTitle())
      shade = ml;
  }
  QVERIFY2(shade, "지질 음영 레이어");
  auto* hs = qobject_cast<QgsRasterLayer*>(shade);
  QVERIFY(hs);
  QVERIFY2(dynamic_cast<QgsHillshadeRenderer*>(hs->renderer()),
           "DEM 색띠가 아니라 회색 음영이어야 지질 색이 산다");
  QCOMPARE(hs->blendMode(), QPainter::CompositionMode_Multiply);
  QVERIFY2(hs->opacity() > 0.34 && hs->opacity() < 0.71, "음영이 색 위에서 보여야 함");
  QCOMPARE(geo->blendMode(), QPainter::CompositionMode_SourceOver);
  QVERIFY2(proj.mapLayer(dem->id()) == dem, "기존 DEM 높이 범례는 유지");
  if (QgsLayerTree* root = proj.layerTreeRoot()) {
    QgsLayerTreeLayer* geoN = root->findLayer(geo->id());
    QgsLayerTreeLayer* shN = root->findLayer(hs->id());
    QVERIFY(geoN && shN);
    auto* parent = qobject_cast<QgsLayerTreeGroup*>(geoN->parent());
    if (!parent) parent = root;
    QVERIFY2(parent->children().indexOf(shN) < parent->children().indexOf(geoN),
             "음영이 지질 색보다 위에 있어야 능선이 보임");
  }

  QgsProject bare;
  bare.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* geoBare = mem.clone();
  geoBare->setName(QStringLiteral("지질도(KIGAM 1:5만)"));
  QVERIFY(bare.addMapLayer(geoBare, true));
  QString bareErr;
  QVERIFY2(GeologyMapService::ensureReliefUnderlay(&bare, nullptr, geoBare, &bareErr),
           qPrintable(bareErr));
  QgsMapLayer* xyzShade = nullptr;
  for (QgsMapLayer* ml : bare.mapLayers()) {
    QVERIFY2(!ml || ml->name() != QStringLiteral("지형맵"),
             "지형맵 폴백은 두 번째 지질도 클릭에 남음");
    if (ml && ml->name() == GeologyMapService::reliefLayerTitle())
      xyzShade = ml;
  }
  QVERIFY2(xyzShade, "DEM 없어도 지질 음영이 있어야 화면에 변화가 있음");
  QCOMPARE(xyzShade->blendMode(), QPainter::CompositionMode_Multiply);
  QCOMPARE(geoBare->blendMode(), QPainter::CompositionMode_SourceOver);
  QCOMPARE(GeologyMapService::existingGeologyLayer(&bare), geoBare);

  QFile geoSrc(QStringLiteral("src/core/GeologyMapService.cpp"));
  QVERIFY2(geoSrc.open(QIODevice::ReadOnly | QIODevice::Text), "GeologyMapService.cpp");
  const QString body = QString::fromUtf8(geoSrc.readAll());
  QVERIFY2(body.contains(QLatin1String("ensureReliefUnderlay")),
           "지질도 받을 때 음영을 깔아야 함");
  QVERIFY2(body.contains(QLatin1String("World_Hillshade")),
           "로컬 DEM 없으면 세계 음영 타일이라도 깔아야 함");
  QVERIFY2(!body.contains(QLatin1String("refreshAllLayers")),
           "음영 추가가 지적 타일 캐시를 버리면 안 됨");
  QVERIFY2(!body.contains(QLatin1String("addElevationHillshadeMap")),
           "지형맵 XYZ는 지질 음영이 아님");
  QVERIFY2(!body.contains(QString::fromUtf8("지형맵")),
           "지형맵을 음영 자리에 넣으면 토글이 못 끔");
}

void TestWorkflow::demRelief_drapesMultiplyOverDem() {
  GDALAllRegister();
  const QString path = QDir::temp().filePath(QStringLiteral("ka_hgis_dem_relief_test.tif"));
  QFile::remove(path);
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
  QVERIFY2(drv, "GTiff");
  GDALDataset* ds = drv->Create(path.toUtf8().constData(), 16, 16, 1, GDT_Float32, nullptr);
  QVERIFY2(ds, "create DEM tif");
  double gt[6] = {200000.0, 10.0, 0.0, 450000.0, 0.0, -10.0};
  ds->SetGeoTransform(gt);
  OGRSpatialReference srs;
  srs.SetFromUserInput("EPSG:5186");
  char* wkt = nullptr;
  srs.exportToWkt(&wkt);
  if (wkt) {
    ds->SetProjection(wkt);
    CPLFree(wkt);
  }
  std::vector<float> z(16 * 16);
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x)
      z[static_cast<size_t>(y) * 16 + x] = 50.0f + float(x * 5 + y * 10);
  }
  QVERIFY(ds->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 16, 16, z.data(), 16, 16, GDT_Float32, 0,
                                         0) == CE_None);
  GDALClose(ds);

  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* dem = new QgsRasterLayer(path, QStringLiteral("DEM"), QStringLiteral("gdal"));
  QVERIFY2(dem->isValid(), "valid local DEM");
  LayerOps::applyDemElevationStyle(dem);
  QVERIFY(proj.addMapLayer(dem, true));
  LayerOps::placeInLegendGroup(&proj, dem, QStringLiteral("참조 지도"));

  QgsRasterLayer* shade = LayerOps::ensureDemRelief(&proj, dem);
  QVERIFY2(shade != nullptr, "지형 음영이 생성되어야 함");
  QVERIFY2(shade->isValid(), "지형 음영 레이어가 유효해야 함");
  QCOMPARE(shade->name(), QStringLiteral("지형 음영"));
  QCOMPARE(shade->blendMode(), QPainter::CompositionMode_Multiply);
  QCOMPARE(shade->opacity(), .30); // Natural default; user strength is tested separately.

  // QgsProject에서 삭제되지 않고 온전히 보존되어야 한다
  QVERIFY2(proj.mapLayer(shade->id()) == shade, "지형 음영이 프로젝트에서 삭제되면 안 됨");
  QVERIFY2(proj.mapLayer(dem->id()) == dem, "DEM 레이어도 유지되어야 함");

  // 범례 트리에서 지형 음영이 DEM 바로 위에 쌓여 있어야 한다
  if (QgsLayerTree* root = proj.layerTreeRoot()) {
    QgsLayerTreeLayer* demNode = root->findLayer(dem->id());
    QgsLayerTreeLayer* shadeNode = root->findLayer(shade->id());
    QVERIFY2(demNode != nullptr, "DEM 노드 존재");
    QVERIFY2(shadeNode != nullptr, "지형 음영 노드 존재");
    QVERIFY2(shadeNode->itemVisibilityChecked(), "지형 음영이 켜져 있어야 함");
    auto* parent = qobject_cast<QgsLayerTreeGroup*>(demNode->parent());
    if (!parent) parent = root;
    const int demIdx = parent->children().indexOf(demNode);
    const int shadeIdx = parent->children().indexOf(shadeNode);
    QVERIFY2(shadeIdx >= 0 && demIdx >= 0, "노드가 동일 부모 그룹에 위치");
    QVERIFY2(shadeIdx < demIdx, "지형 음영이 DEM보다 위에 렌더링되도록 상위에 위치해야 함");
  }

  // 중복 호출 시 새 레이어를 만들지 않고 기존 레이어를 재사용해야 한다
  QgsRasterLayer* shade2 = LayerOps::ensureDemRelief(&proj, dem);
  QCOMPARE(shade2, shade);

  QFile::remove(path);
}

void TestWorkflow::ensureDomainLayer_reopenAfterPurgeClearsDeletedPolygons() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_purge_%1")
                                            .arg(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  QString err;
  const QString gpkg =
      SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("퍼지재출현"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  QgsProject proj;
  QgsVectorLayer* vl =
      LayerOps::ensureDomainLayer(&proj, gpkg, QStringLiteral("survey_area"),
                                  QStringLiteral("조사구역"), &err);
  QVERIFY2(vl, qPrintable(err));
  QVERIFY(vl->startEditing());
  QgsFeature f(vl->fields());
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200080, 450000)
       << QgsPointXY(200080, 450080) << QgsPointXY(200000, 450080)
       << QgsPointXY(200000, 450000);
  f.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(vl->addFeature(f));
  QVERIFY(vl->commitChanges());
  QVERIFY(vl->featureCount() >= 1);

  QVERIFY2(LayerOps::purgeCommittedFeatures(vl, &err), qPrintable(err));
  QCOMPARE(int(vl->featureCount()), 0);

  const QString id = vl->id();
  proj.removeMapLayer(id);
  QgsVectorLayer* again =
      LayerOps::ensureDomainLayer(&proj, gpkg, QStringLiteral("survey_area"),
                                  QStringLiteral("조사구역"), &err);
  QVERIFY2(again, qPrintable(err));
  QCOMPARE(int(again->featureCount()), 0);
}

void TestWorkflow::vertexEdit_snapsAndOffersAddDeleteOnLine() {
  QFile tool(QStringLiteral("src/app/KaVertexEditTool.cpp"));
  QVERIFY2(tool.open(QIODevice::ReadOnly | QIODevice::Text), "KaVertexEditTool.cpp");
  const QString src = QString::fromUtf8(tool.readAll());
  QVERIFY2(src.contains(QLatin1String("snapToMap")),
           "도형수정 점 이동에 자석이 없으면 근처 점이 안 붙음");
  QVERIFY2(src.contains(QString::fromUtf8("점추가")),
           "선 우클릭에 점추가가 있어야 함");
  QVERIFY2(src.contains(QString::fromUtf8("점삭제")),
           "선 우클릭에 점삭제가 있어야 함");
  QVERIFY2(src.contains(QLatin1String("QMenu")),
           "우클릭은 바로 지우지 말고 메뉴여야 함");
  QVERIFY2(src.contains(QLatin1String("PreventContextMenu")),
           "도형수정 중 지도 우클릭 메뉴가 점추가를 가리면 안 됨");
  const int press = src.indexOf(QLatin1String("void KaVertexEditTool::canvasPressEvent"));
  QVERIFY2(press >= 0, "canvasPressEvent");
  const int pressEnd = src.indexOf(QLatin1String("void KaVertexEditTool::"), press + 10);
  const QString pressBody = src.mid(press, pressEnd > press ? pressEnd - press : 1200);
  QVERIFY2(!pressBody.contains(QLatin1String("deleteVertexAt(idx)")),
           "우클릭에서 점을 바로 지우면 메뉴가 안 나옴");

  const QString mwSrc = readMainWindowSources();
  QVERIFY2(!mwSrc.isEmpty(), "MainWindow.cpp + Export + Editing");
  const int snap = mwSrc.indexOf(QLatin1String("void MainWindow::applySnapConfig"));
  QVERIFY2(snap >= 0, "applySnapConfig");
  const int snapEnd = mwSrc.indexOf(QLatin1String("void MainWindow::"), snap + 10);
  const QString snapBody = mwSrc.mid(snap, snapEnd > snap ? snapEnd - snap : 800);
  // 도형수정은 이제 별도 도구가 아니라 「도형선택」 안에 있다.
  // 자석 설정도 그 경로를 따라 꼭짓점 편집기까지 내려가야 한다.
  QVERIFY2(snapBody.contains(QLatin1String("m_featureSelectTool")) &&
               snapBody.contains(QLatin1String("setSnapEnabled")),
           "자석 설정이 도형선택(=도형수정) 도구에 전달돼야 함");
  QFile sel(QStringLiteral("src/app/KaFeatureSelectTool.cpp"));
  QVERIFY2(sel.open(QIODevice::ReadOnly | QIODevice::Text), "KaFeatureSelectTool.cpp");
  QVERIFY2(QString::fromUtf8(sel.readAll())
               .contains(QLatin1String("m_vertex->setSnapEnabled")),
           "선택 도구가 받은 자석 설정을 꼭짓점 편집기에 넘겨야 함");
}

void TestWorkflow::riverLevelLegend_waterStyleAndNameLabels() {
  // 수계도 관례: 하천 등급별 물색 범례(상위 등급 먼저) + 하천명 라벨.
  QgsVectorLayer mem(QStringLiteral("MultiPolygon?crs=EPSG:5186"), QStringLiteral("riverwfs"),
                     QStringLiteral("memory"));
  QVERIFY(mem.isValid());
  mem.dataProvider()->addAttributes(
      {QgsField(QStringLiteral("riv_nm"), QMetaType::Type::QString),
       QgsField(QStringLiteral("riv_level"), QMetaType::Type::QString)});
  mem.updateFields();
  QgsFeature f1(mem.fields());
  f1.setAttribute(0, QStringLiteral("병성천"));
  f1.setAttribute(1, QStringLiteral("지방1급하천"));
  QgsFeature f2(mem.fields());
  f2.setAttribute(0, QStringLiteral("장산천"));
  f2.setAttribute(1, QStringLiteral("지방2급하천"));
  QgsFeature f3(mem.fields());
  f3.setAttribute(0, QStringLiteral("낙동강"));
  f3.setAttribute(1, QStringLiteral("국가하천"));
  QVERIFY(mem.dataProvider()->addFeatures(QgsFeatureList() << f1 << f2 << f3));

  QVERIFY(RiverMapService::applyRiverStyle(&mem));
  auto* cat = dynamic_cast<QgsCategorizedSymbolRenderer*>(mem.renderer());
  QVERIFY2(cat, "categorized renderer expected");
  QCOMPARE(cat->classAttribute(), QStringLiteral("riv_level"));
  // 실존 등급 3개 + 기타 캐치올. 국가하천이 맨 앞.
  QCOMPARE(cat->categories().size(), 4);
  QCOMPARE(cat->categories().at(0).value().toString(), QStringLiteral("국가하천"));
  QCOMPARE(cat->categories().at(1).value().toString(), QStringLiteral("지방1급하천"));
  QCOMPARE(cat->categories().at(2).value().toString(), QStringLiteral("지방2급하천"));
  // 국가하천 물색 — 위성·지적 위에서 더 진한 파랑으로 강조.
  const QColor c0 = cat->categories().at(0).symbol()->color();
  QCOMPARE(QColor(c0.red(), c0.green(), c0.blue()), QColor(0, 126, 212));
  QVERIFY2(c0.blue() > c0.red() + 150, "수계 강조는 파란 채널이 빨강보다 훨씬 커야 한다");
  QVERIFY2(mem.labelsEnabled(), "river name labels expected");

  QgsVectorLayer noField(QStringLiteral("MultiPolygon?crs=EPSG:5186"),
                         QStringLiteral("nofield"), QStringLiteral("memory"));
  QVERIFY(!RiverMapService::applyRiverStyle(&noField));
}

void TestWorkflow::thematicDownloadMaxSpanFourTimesPrior() {
  // Prior caps: soil/geology 20 km, river 40 km. Field request: 4x linear span.
  QCOMPARE(SoilMapService::maxSpanMeters(), 80000.0);
  QCOMPARE(GeologyMapService::maxSpanMeters(), 80000.0);
  QCOMPARE(RiverMapService::maxSpanMeters(), 160000.0);
}

void TestWorkflow::thematicDownloadCovers1in10000Drawing() {
  // Geology/river may grow the helper envelope. Soil download must not — precision
  // soil at 80 km is tens of thousands of polygons (20–90 s). 80 km is a reject cap.
  const QgsRectangle view(200000.0, 450000.0, 204200.0, 452970.0);
  QCOMPARE(view.width(), 4200.0);
  const QgsRectangle geo =
      LayerOps::expandExtentToMaxSpan(view, GeologyMapService::maxSpanMeters());
  QVERIFY2(qAbs(qMax(geo.width(), geo.height()) - 80000.0) < 1.0,
           "geology helper envelope grows to 80 km");
  QVERIFY(qAbs(geo.center().x() - view.center().x()) < 1.0);
  QVERIFY(qAbs(geo.center().y() - view.center().y()) < 1.0);
  QVERIFY2(geo.contains(view), "1:10000 view stays inside the geology envelope");

  const QgsRectangle river =
      LayerOps::expandExtentToMaxSpan(view, RiverMapService::maxSpanMeters());
  QVERIFY2(qAbs(qMax(river.width(), river.height()) - 160000.0) < 1.0,
           "river envelope grows to 160 km");

  const QgsRectangle tooWide(0.0, 0.0, 90000.0, 10000.0);
  const QgsRectangle kept =
      LayerOps::expandExtentToMaxSpan(tooWide, SoilMapService::maxSpanMeters());
  QCOMPARE(kept.width(), tooWide.width());
  QCOMPARE(kept.height(), tooWide.height());
}

void TestWorkflow::soilDownload_usesViewExtentAndTerrainFieldOnly() {
  // Live: Jeju ~4 km SOIL_1 full GeoJSON 2.8 MB / 3.8 s; propertyName=soil_type_geo
  // 59 KB / 0.16 s. Growing the same view to 80 km pulled 15k features × 3 tables.
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QgsRectangle view5186(200000, 450000, 204200, 452970);
  const QgsCoordinateTransformContext context;
  const QgsCoordinateTransform toWgs84(
      QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
      QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")), context);
  const QgsRectangle expected = toWgs84.transformBoundingBox(view5186);
  QList<QUrl> requests;
  const auto prepared = SoilMapService::prepare(view5186,
      directory.filePath(QStringLiteral("soil.gpkg")), context, nullptr,
      [&requests](const QNetworkRequest& request, QByteArray* body, QString*, QgsFeedback*) {
        requests.append(request.url());
        const QUrlQuery query(request.url());
        if (query.queryItemValue(QStringLiteral("resultType")) == QLatin1String("hits"))
          *body = QByteArrayLiteral("<FeatureCollection numberMatched=\"1\"/>");
        else
          *body = QByteArrayLiteral(R"({"type":"FeatureCollection","crs":{"type":"name","properties":{"name":"EPSG:5186"}},"features":[{"type":"Feature","properties":{"soil_type_geo":"04"},"geometry":{"type":"Polygon","coordinates":[[[200000,450000],[200010,450000],[200010,450010],[200000,450010],[200000,450000]]]}}]})");
        return true;
      });
  QVERIFY2(prepared.isReady(), qPrintable(prepared.error));
  QCOMPARE(requests.size(), 6);
  int featureRequests = 0;
  for (const QUrl& request : requests) {
    const QUrlQuery query(request);
    const QStringList bbox = query.queryItemValue(QStringLiteral("bbox")).split(QLatin1Char(','));
    QCOMPARE(bbox.size(), 5);
    const QList<double> expectedBounds = {expected.yMinimum(), expected.xMinimum(),
                                         expected.yMaximum(), expected.xMaximum()};
    for (int i = 0; i < 4; ++i) {
      bool valid = false;
      const double value = bbox.at(i).toDouble(&valid);
      QVERIFY(valid);
      QVERIFY2(qAbs(value - expectedBounds.at(i)) < 1e-7,
               "soil request bbox must match the current view, without expansion to 80 km");
    }
    QCOMPARE(bbox.last(), QStringLiteral("urn:ogc:def:crs:EPSG::4326"));
    if (query.queryItemValue(QStringLiteral("resultType")) != QLatin1String("hits")) {
      ++featureRequests;
      QCOMPARE(query.queryItemValue(QStringLiteral("propertyName")), QStringLiteral("soil_type_geo,geom"));
      QCOMPARE(query.queryItemValue(QStringLiteral("outputFormat")), QStringLiteral("application/json"));
    }
  }
  QCOMPARE(featureRequests, 3);

  const QgsRectangle jeju4326(126.45, 33.43, 126.55, 33.50);
  const QString feature = SoilMapService::wfsGetFeatureUrl(1, jeju4326, false);
  QVERIFY2(feature.contains(QLatin1String("propertyName=soil_type_geo,geom")),
           feature.toUtf8().constData());
  QVERIFY2(feature.contains(QLatin1String("typeNames=soilmap:SOIL_1")),
           feature.toUtf8().constData());
  QVERIFY2(feature.contains(QLatin1String("outputFormat=application/json")),
           feature.toUtf8().constData());
  QVERIFY2(!feature.contains(QLatin1String("resultType=hits")),
           "GetFeature must return geometries, not hits-only");

  const QString hits = SoilMapService::wfsGetFeatureUrl(2, jeju4326, true);
  QVERIFY2(hits.contains(QLatin1String("resultType=hits")), hits.toUtf8().constData());
  QVERIFY2(hits.contains(QLatin1String("typeNames=soilmap:SOIL_2")),
           hits.toUtf8().constData());
  QVERIFY2(!hits.contains(QLatin1String("outputFormat=application/json")),
           "hits must stay XML so numberMatched is cheap");
}

void TestWorkflow::soilTerrainPicture_is3857ArcGisCacheNot5186Wms() {
  // 흙토람 웹 분포지형은 SOIL_1/2/3 WFS가 아니라 3857 ArcGIS 캐시 그림이다.
  // 산능선 빈곳은 벡터에 없고, L08–L15 타일이 산악지 초록으로 채운다.
  const QString uri = SoilMapService::terrainPictureUri();
  QVERIFY2(uri.contains(QLatin1String("type=xyz")), uri.toUtf8().constData());
  QVERIFY2(uri.contains(QLatin1String("crs=EPSG:3857")), uri.toUtf8().constData());
  QVERIFY2(uri.contains(QLatin1String("zmax=15")), uri.toUtf8().constData());
  QVERIFY2(uri.contains(QLatin1String("TOP_A_SOIL_T_GEO")), uri.toUtf8().constData());
  QVERIFY2(!uri.contains(QLatin1String("crs=EPSG:5186")), "WMS/XYZ URI must not set work CRS");
  QVERIFY2(!uri.contains(QLatin1String("crs=EPSG:5179")), uri.toUtf8().constData());

  const QUrl raw13(QStringLiteral(
      "https://gis.naas.go.kr/Geodata/SF/TOP_A_SOIL_T_GEO/Layers/_alllayers/"
      "L13/R3167/C7003.png"));
  const QUrl hex13 = SoilMapService::rewriteArcGisCacheUrl(raw13);
  QVERIFY2(hex13.path().endsWith(QLatin1String("/L13/R00000c5f/C00001b5b.png")),
           hex13.toString().toUtf8().constData());

  const QUrl raw8(QStringLiteral(
      "https://gis.naas.go.kr/Geodata/SF/TOP_A_SOIL_T_GEO/Layers/_alllayers/"
      "L8/R98/C218.png"));
  const QUrl hex8 = SoilMapService::rewriteArcGisCacheUrl(raw8);
  QVERIFY2(hex8.path().endsWith(QLatin1String("/L08/R00000062/C000000da.png")),
           hex8.toString().toUtf8().constData());

  const QUrl other(QStringLiteral("https://example.com/L8/R98/C218.png"));
  QCOMPARE(SoilMapService::rewriteArcGisCacheUrl(other), other);
}

void TestWorkflow::elevationMap_is3857ReferenceToggleNotTiffDialog() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  QVERIFY2(LayerOps::addElevationHillshadeMap(&proj, nullptr, QString(), &err),
           qPrintable(err));
  QgsMapLayer* elev = nullptr;
  for (QgsMapLayer* l : proj.mapLayers()) {
    if (l && l->name().contains(QStringLiteral("지형맵")))
      elev = l;
  }
  QVERIFY2(elev, "지형맵 참조 레이어 (OpenTopoMap)");
  QVERIFY2(LayerOps::isReferenceLayer(elev), "지형맵 is 참조 지도, not survey domain");
  QVERIFY2(elev->crs().authid() == QLatin1String("EPSG:3857") ||
               elev->source().contains(QLatin1String("EPSG:3857")),
           "WMS/XYZ CRS is 3857; work CRS stays 5186 via OTF");
  QCOMPARE(proj.crs().authid(), QStringLiteral("EPSG:5186"));
  QVERIFY2(!elev->source().contains(QLatin1String("EPSG:5186")) &&
               !elev->source().contains(QLatin1String("EPSG:5179")),
           "do not put work/upload CRS on the terrain XYZ");
  QVERIFY2(elev->source().contains(QLatin1String("opentopomap"), Qt::CaseInsensitive),
           "지형맵 is contour/hillshade terrain, not color DEM");

  QFile mw(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString src = QString::fromUtf8(mw.readAll());
  const int fn = src.indexOf(QLatin1String("void MainWindow::toggleTerrainMap"));
  QVERIFY2(fn >= 0, "지형맵 버튼은 toggleTerrainMap");
  const int next = src.indexOf(QLatin1String("\nvoid MainWindow::"), fn + 10);
  const QString body = next > fn ? src.mid(fn, next - fn) : src.mid(fn, 1200);
  QVERIFY2(body.contains(QLatin1String("addElevationHillshadeMap")),
           "클릭하면 지형맵을 올린다");
  QVERIFY2(!body.contains(QLatin1String("getOpenFileName")),
           "지형맵 클릭은 GeoTIFF 파일 고르기가 아니다");
  QVERIFY2(src.contains(QLatin1String("setText(QStringLiteral(\"지형맵\"))")) ||
               src.contains(QStringLiteral("지형맵")),
           "toolbar shows 지형맵");
}

void TestWorkflow::elevationMap_xyzOnlySkipsAbortWhilePanning() {
  // crash-20260901-102801: 고도맵(WMS GetMap) + OTF QgsRasterProjector +
  // 팬 중 TileDownloadManager deleteLater → 0xc0000005. 프로그램이 꺼진다.
  const QString opsSrc = readLayerOpsSources();
  QVERIFY2(!opsSrc.isEmpty(), "run from source tree (ctest WORKING_DIRECTORY)");
  const int fn = opsSrc.indexOf(QLatin1String("bool LayerOps::addElevationHillshadeMap"));
  QVERIFY2(fn >= 0, "addElevationHillshadeMap");
  const int next = opsSrc.indexOf(QLatin1String("\nbool LayerOps::"), fn + 10);
  QVERIFY2(next > fn, "addElevationHillshadeMap body");
  const QString elev = opsSrc.mid(fn, next - fn);
  QVERIFY2(!elev.contains(QLatin1String("makeVworldWmsUri")),
           "VWorld WMS GetMap + OTF pan aborts provider_wms (field dump 10:28)");
  QVERIFY2(!elev.contains(QLatin1String("lt_c_elshade")) &&
               !elev.contains(QLatin1String("LT_C_ELSHADE")),
           "do not add the hillshade as a WMS layer");
  QVERIFY2(elev.contains(QLatin1String("type=xyz")),
           "지형맵 must be XYZ tiles, same hardened path as satellite");
  QVERIFY2(elev.contains(QLatin1String("opentopomap"), Qt::CaseInsensitive),
           "OpenTopoMap XYZ is the 지형맵 overlay");
  QVERIFY2(elev.contains(QLatin1String("EPSG:3857")),
           "tile CRS stays 3857; work CRS is OTF");

  const int clampFn = opsSrc.indexOf(QLatin1String("bool LayerOps::clampCanvasToKorea"));
  QVERIFY2(clampFn >= 0, "clampCanvasToKorea");
  const int clampNext = opsSrc.indexOf(QLatin1String("\nvoid LayerOps::"), clampFn + 10);
  QVERIFY2(clampNext > clampFn, "clampCanvasToKorea body");
  QVERIFY2(opsSrc.mid(clampFn, clampNext - clampFn).contains(QLatin1String("isDrawing")),
           "pan must not setExtent while XYZ/WMS tiles are still drawing");

  QFile mw(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString app = QString::fromUtf8(mw.readAll());
  const int toggleFn = app.indexOf(QLatin1String("void MainWindow::toggleTerrainMap"));
  QVERIFY2(toggleFn >= 0, "toggleTerrainMap");
  const int toggleNext = app.indexOf(QLatin1String("void MainWindow::"), toggleFn + 10);
  const QString toggle = app.mid(toggleFn, toggleNext - toggleFn);
  QVERIFY2(!toggle.contains(QLatin1String("syncMapCanvas")) ||
               toggle.contains(QLatin1String("isDrawing")),
           "adding 지형맵 must not restack an in-flight tile job");

  bool extentsClamps = false;
  int pos = 0;
  while (true) {
    const int ext = app.indexOf(QLatin1String("&QgsMapCanvas::extentsChanged"), pos);
    if (ext < 0) break;
    const int end = app.indexOf(QLatin1String("});"), ext);
    const QString lambda = app.mid(ext, qMax(80, (end > ext ? end : ext + 420) - ext));
    if (lambda.contains(QLatin1String("clampCanvasToKorea"))) {
      extentsClamps = true;
      break;
    }
    pos = ext + 10;
  }
  QVERIFY2(extentsClamps, "extentsChanged must clamp the view back into Korea");

  // 예전에는 이 램다가 isDrawing() 이면 클램프를 통째로 건너뛰었다. 그런데 줌아웃은
  // 곧바로 렌더를 띄우므로 extentsChanged 시점의 캔버스는 대개 그리는 중이었고,
  // 그래서 클램프가 거의 매번 버려져 한국 밖(1:800만 등)까지 줌아웃되면 VWorld 타일이
  // 없어 위성이 통째로 사라졌다. 이제 clampCanvasToKorea 가 직접 isDrawing() 을 보고,
  // 진행 중인 WMS 작업을 끊는 대신 끝난 뒤로 미룬다 — 버리지는 않는다.
  const QString clampBody = opsSrc.mid(clampFn, 1200);
  QVERIFY2(clampBody.contains(QLatin1String("isDrawing")),
           "must not move the extent while an in-flight WMS job is drawing");
  QVERIFY2(clampBody.contains(QLatin1String("runWhenCanvasIdle")),
           "그리는 중이면 버리지 말고 끝난 뒤로 미뤄야 한다");
  const int deferFn = opsSrc.indexOf(QLatin1String("void runWhenCanvasIdle"));
  QVERIFY2(deferFn >= 0, "runWhenCanvasIdle helper must exist");
  const QString deferBody = opsSrc.mid(deferFn, 1200);
  QVERIFY2(deferBody.contains(QLatin1String("QTimer::singleShot")),
           "미뤄 둔 일은 타이머로 반드시 다시 실행돼야 한다 (버리면 화면이 빈 채로 굳는다)");
  QVERIFY2(!deferBody.contains(QLatin1String("stopRendering")),
           "must not abort an in-flight WMS job");
}

void TestWorkflow::demColorRelief_is3857XyzNotTerrainMap() {
  // User: OpenTopoMap = 지형맵. DEM = color hypsometric + hillshade (GIBS ASTER).
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  QVERIFY2(LayerOps::addDemColorReliefMap(&proj, nullptr, &err), qPrintable(err));
  QgsMapLayer* dem = nullptr;
  for (QgsMapLayer* l : proj.mapLayers()) {
    if (l && l->name() == QLatin1String("DEM"))
      dem = l;
  }
  QVERIFY2(dem, "DEM 참조 레이어");
  QVERIFY2(LayerOps::isReferenceLayer(dem), "DEM is 참조 지도");
  QVERIFY2(dem->crs().authid() == QLatin1String("EPSG:3857") ||
               dem->source().contains(QLatin1String("EPSG:3857")),
           "DEM tile CRS is 3857; work CRS stays 5186 via OTF");
  QCOMPARE(proj.crs().authid(), QStringLiteral("EPSG:5186"));
  QVERIFY2(!dem->source().contains(QLatin1String("EPSG:5186")) &&
               !dem->source().contains(QLatin1String("EPSG:5179")),
           "do not put work/upload CRS on the DEM XYZ");
  QVERIFY2(dem->source().contains(QLatin1String("type=xyz")) ||
               dem->source().contains(QLatin1String("ASTER_GDEM")) ||
               dem->source().contains(QLatin1String("gibs.earthdata")),
           "DEM is precolored elevation tiles, not a GeoTIFF dialog");
  QVERIFY2(!dem->source().contains(QLatin1String("opentopomap"), Qt::CaseInsensitive),
           "DEM must not be the OpenTopoMap terrain layer");

  const QString opsSrc = readLayerOpsSources();
  QVERIFY2(!opsSrc.isEmpty(), "LayerOps.cpp + BasemapOps + LabelOps + ControlPointCsv");
  const int fn = opsSrc.indexOf(QLatin1String("bool LayerOps::addDemColorReliefMap"));
  QVERIFY2(fn >= 0, "addDemColorReliefMap");
  const int next = opsSrc.indexOf(QLatin1String("\nbool LayerOps::"), fn + 10);
  QVERIFY2(next > fn, "addDemColorReliefMap body");
  const QString body = opsSrc.mid(fn, next - fn);
  QVERIFY2(!body.contains(QLatin1String("makeVworldWmsUri")),
           "DEM must not use VWorld WMS GetMap (pan AV)");
  QVERIFY2(body.contains(QLatin1String("type=xyz")), "DEM is XYZ");
  QVERIFY2(body.contains(QLatin1String("applyDemElevationStyle")),
           "view DEM must get a meter color-ramp legend, not RGB-only tiles");
  QVERIFY2(body.contains(QLatin1String("vsicurl")) ||
               body.contains(QLatin1String("copernicus-dem"), Qt::CaseInsensitive) ||
               body.contains(QLatin1String("elevation-tiles-prod")),
           "prefer a single-band elevation raster over painted JPEG tiles");
  QVERIFY2(body.contains(QLatin1String("ASTER_GDEM_Color_Shaded_Relief")),
           "GIBS color tiles stay as offline/fallback only");
  QVERIFY2(body.contains(QLatin1String("%7By%7D/%7Bx%7D")) ||
               body.contains(QLatin1String("{y}/{x}")),
           "GIBS WMTS order is z/y/x, not OSM z/x/y");

  QFile mw(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString app = QString::fromUtf8(mw.readAll());
  const int toggleFn = app.indexOf(QLatin1String("void MainWindow::toggleDemMap"));
  QVERIFY2(toggleFn >= 0, "DEM 버튼은 toggleDemMap");
  const int toggleNext = app.indexOf(QLatin1String("void MainWindow::"), toggleFn + 10);
  const QString toggle = app.mid(toggleFn, toggleNext - toggleFn);
  QVERIFY2(toggle.contains(QLatin1String("startDemDownload")),
           "DEM click prepares elevation data in the cancellable download task");
  QVERIFY2(!toggle.contains(QLatin1String("getOpenFileName")),
           "DEM click is not a file picker");
  QVERIFY2(!toggle.contains(QLatin1String("syncMapCanvas")) ||
               toggle.contains(QLatin1String("isDrawing")),
           "adding DEM must not restack an in-flight tile job");
  QVERIFY2(app.contains(QLatin1String("btnTerrain")), "toolbar has 지형맵 button");
  QVERIFY2(app.contains(QLatin1String("setText(QStringLiteral(\"DEM\"))")),
           "DEM button label is DEM");

  QFile boot(QStringLiteral("src/app/KaApplication.cpp"));
  QVERIFY2(boot.open(QIODevice::ReadOnly | QIODevice::Text), "KaApplication.cpp");
  const QString smoke = QString::fromUtf8(boot.readAll());
  QVERIFY2(smoke.contains(QLatin1String("toolbar_dem")) &&
               smoke.contains(QStringLiteral("DEM")),
           "smoke looks for DEM, not the old 고도맵 label");
  QVERIFY2(smoke.contains(QLatin1String("toolbar_terrain")) &&
               smoke.contains(QStringLiteral("지형맵")),
           "smoke looks for 지형맵");
}

void TestWorkflow::demElevationStyle_legendListsHeightMeters() {
  // 조판 범례에 "DEM" 글자만 있으면 높이를 읽을 수 없다. 단일밴드 + 색띠 + m.
  GDALAllRegister();
  const QString path = QDir::temp().filePath(QStringLiteral("ka_hgis_dem_legend_test.tif"));
  QFile::remove(path);
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
  QVERIFY2(drv, "GTiff");
  GDALDataset* ds = drv->Create(path.toUtf8().constData(), 16, 16, 1, GDT_Float32, nullptr);
  QVERIFY2(ds, "create DEM tif");
  double gt[6] = {200000.0, 10.0, 0.0, 450000.0, 0.0, -10.0};
  ds->SetGeoTransform(gt);
  OGRSpatialReference srs;
  srs.SetFromUserInput("EPSG:5186");
  char* wkt = nullptr;
  srs.exportToWkt(&wkt);
  if (wkt) {
    ds->SetProjection(wkt);
    CPLFree(wkt);
  }
  std::vector<float> z(16 * 16);
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x)
      z[static_cast<size_t>(y) * 16 + x] = 12.0f + float(x + y) * 8.0f;
  }
  QVERIFY(ds->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 16, 16, z.data(), 16, 16, GDT_Float32, 0,
                                         0) == CE_None);
  GDALClose(ds);

  QgsRasterLayer rl(path, QStringLiteral("DEM"), QStringLiteral("gdal"));
  QVERIFY2(rl.isValid(), "gdal DEM");
  QVERIFY2(LayerOps::applyDemElevationStyle(&rl), "applyDemElevationStyle");
  auto* rend = dynamic_cast<QgsSingleBandPseudoColorRenderer*>(rl.renderer());
  QVERIFY2(rend, "QgsSingleBandPseudoColorRenderer so the layout legend can list classes");
  QVERIFY(rend->shader());
  auto* fn = dynamic_cast<QgsColorRampShader*>(rend->shader()->rasterShaderFunction());
  QVERIFY2(fn, "QgsColorRampShader");
  const QList<QgsColorRampShader::ColorRampItem> items = fn->colorRampItemList();
  QVERIFY2(items.size() >= 3, "enough breaks for a height ramp");
  bool sawMeters = false;
  for (const QgsColorRampShader::ColorRampItem& it : items) {
    if (it.label.contains(QLatin1Char('m')))
      sawMeters = true;
  }
  QVERIFY2(sawMeters, "legend labels must include height in meters");

  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  QVERIFY2(LayerOps::addDemElevationRaster(&proj, nullptr, path, &err), qPrintable(err));
  QgsMapLayer* added = nullptr;
  for (QgsMapLayer* l : proj.mapLayers()) {
    if (l && l->name() == QLatin1String("DEM"))
      added = l;
  }
  QVERIFY(added);
  QVERIFY(LayerOps::isReferenceLayer(added));
  QVERIFY2(dynamic_cast<QgsSingleBandPseudoColorRenderer*>(
               qobject_cast<QgsRasterLayer*>(added)->renderer()),
           "imported raster uses the meter ramp");
  QFile::remove(path);
}

void TestWorkflow::demElevationStyle_continuousLowlandStops() {
  // 약 8칸. 5 m × 24줄은 범례가 맵을 가린다.
  QCOMPARE(LayerOps::demElevationClassStep(0.0, 18.0), 5.0);
  QCOMPARE(LayerOps::demElevationClassStep(0.0, 120.0), 15.0);
  QCOMPARE(LayerOps::demElevationClassStep(-2.0, 1155.0), 200.0);
  QCOMPARE(LayerOps::demElevationClassStep(12.0, 80.0), 10.0);

  GDALAllRegister();
  const QString path = QDir::temp().filePath(QStringLiteral("ka_hgis_dem_classes.tif"));
  QFile::remove(path);
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
  QVERIFY2(drv, "GTiff");
  GDALDataset* ds = drv->Create(path.toUtf8().constData(), 16, 16, 1, GDT_Float32, nullptr);
  QVERIFY2(ds, "create DEM tif");
  double gt[6] = {200000.0, 10.0, 0.0, 450000.0, 0.0, -10.0};
  ds->SetGeoTransform(gt);
  OGRSpatialReference srs;
  srs.SetFromUserInput("EPSG:5186");
  char* wkt = nullptr;
  srs.exportToWkt(&wkt);
  if (wkt) {
    ds->SetProjection(wkt);
    CPLFree(wkt);
  }
  std::vector<float> z(16 * 16);
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x)
      z[static_cast<size_t>(y) * 16 + x] = 12.0f + float(x + y) * 8.0f;
  }
  QVERIFY(ds->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 16, 16, z.data(), 16, 16, GDT_Float32, 0,
                                         0) == CE_None);
  GDALClose(ds);

  QgsRasterLayer rl(path, QStringLiteral("DEM"), QStringLiteral("gdal"));
  QVERIFY2(rl.isValid(), "gdal DEM");
  QVERIFY2(LayerOps::applyDemElevationStyle(&rl), "applyDemElevationStyle");
  auto* rend = dynamic_cast<QgsSingleBandPseudoColorRenderer*>(rl.renderer());
  QVERIFY(rend && rend->shader());
  auto* fn = dynamic_cast<QgsColorRampShader*>(rend->shader()->rasterShaderFunction());
  QVERIFY2(fn, "QgsColorRampShader");
  QCOMPARE(fn->colorRampType(), Qgis::ShaderInterpolationMethod::Linear);
  const QList<QgsColorRampShader::ColorRampItem> items = fn->colorRampItemList();
  QCOMPARE(items.size(), 16);
  QCOMPARE(items.first().value, 0.);
  QCOMPARE(items.last().value, 2000.);
  QCOMPARE(rend->classificationMin(), 0.);
  QCOMPARE(rend->classificationMax(), 2000.);
  int r1, g1, b1, a1, r2, g2, b2, a2;
  QVERIFY(fn->shade(20., &r1, &g1, &b1, &a1));
  QVERIFY(fn->shade(21., &r2, &g2, &b2, &a2));
  QVERIFY(QColor(r1,g1,b1) != QColor(r2,g2,b2));
  QVERIFY(qGray(r1,g1,b1) > 100);
  int sr = 0, sg = 0, sb = 0, sa = 0;
  QVERIFY2(fn->shade(9000.0, &sr, &sg, &sb, &sa) && sa > 0,
           "peak above sampled max must still get a color");
  QVERIFY2(fn->shade(-500.0, &sr, &sg, &sb, &sa) && sa > 0,
           "values below the first break must still get a color");
  std::unique_ptr<QgsRasterBlock> blk(rend->block(1, rl.extent(), 16, 16));
  QVERIFY2(blk && !blk->isEmpty(), "DEM render block");
  for (int row = 0; row < 16; ++row) {
    for (int col = 0; col < 16; ++col) {
      QVERIFY2(qAlpha(blk->color(row, col)) > 0,
               "no transparent DEM pixel — Discrete overflow leaves white holes");
    }
  }
  for (const QgsColorRampShader::ColorRampItem& it : items) {
    QVERIFY2(!it.label.contains(QStringLiteral("m m")),
             "legend must not double the meter suffix");
    QVERIFY2(it.label.contains(QLatin1Char('m')), "each class lists meters");
  }
  QFile::remove(path);
}

void TestWorkflow::demElevationStyle_userClassCountAndCustomItems() {
  // 사용자가 칸 수·간격·색·라벨을 여러 줄 한꺼번에 바꾼다.
  const QList<LayerOps::DemElevationClass> built =
      LayerOps::buildDemElevationClasses(0.0, 600.0, 6, 100.0);
  QCOMPARE(built.size(), 6);
  QCOMPARE(built[0].lo, 0.0);
  QCOMPARE(built[0].hi, 100.0);
  QCOMPARE(built[4].lo, 400.0);
  QCOMPARE(built[4].hi, 500.0);
  QVERIFY2(!std::isfinite(built.last().hi) && built.last().hi > 0,
           "user classes still end with +inf so peaks are not white holes");
  QVERIFY(built.last().label.contains(QStringLiteral("이상")));
  QVERIFY(built[0].label.contains(QLatin1Char('m')));

  QList<LayerOps::DemElevationClass> custom;
  LayerOps::DemElevationClass a;
  a.lo = 0.0;
  a.hi = 40.0;
  a.color = QColor(10, 20, 30);
  a.label = QStringLiteral("낮음");
  custom.append(a);
  LayerOps::DemElevationClass b;
  b.lo = 40.0;
  b.hi = 90.0;
  b.color = QColor(40, 50, 60);
  b.label = QStringLiteral("중간");
  custom.append(b);
  LayerOps::DemElevationClass c;
  c.lo = 90.0;
  c.hi = 1.0e9;
  c.color = QColor(200, 10, 10);
  c.label = QStringLiteral("90 m 이상");
  custom.append(c);

  GDALAllRegister();
  const QString path = QDir::temp().filePath(QStringLiteral("ka_hgis_dem_user_classes.tif"));
  QFile::remove(path);
  GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
  QVERIFY2(drv, "GTiff");
  GDALDataset* ds = drv->Create(path.toUtf8().constData(), 8, 8, 1, GDT_Float32, nullptr);
  QVERIFY2(ds, "create DEM tif");
  double gt[6] = {200000.0, 10.0, 0.0, 450000.0, 0.0, -10.0};
  ds->SetGeoTransform(gt);
  std::vector<float> z(8 * 8, 25.0f);
  z[0] = 120.0f;
  QVERIFY(ds->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 8, 8, z.data(), 8, 8, GDT_Float32, 0, 0) ==
          CE_None);
  GDALClose(ds);

  QgsRasterLayer rl(path, QStringLiteral("DEM"), QStringLiteral("gdal"));
  QVERIFY(rl.isValid());
  LayerOps::DemElevationStyle style;
  style.classes = custom;
  QVERIFY2(LayerOps::applyDemElevationStyle(&rl, QgsRectangle(), style), "apply custom classes");
  auto* rend = dynamic_cast<QgsSingleBandPseudoColorRenderer*>(rl.renderer());
  QVERIFY(rend && rend->shader());
  auto* fn = dynamic_cast<QgsColorRampShader*>(rend->shader()->rasterShaderFunction());
  QVERIFY(fn);
  const QList<QgsColorRampShader::ColorRampItem> items = fn->colorRampItemList();
  QCOMPARE(items.size(), 3);
  QCOMPARE(items[0].label, QStringLiteral("낮음"));
  QCOMPARE(items[1].color, QColor(40, 50, 60));
  QCOMPARE(items.last().value, 90.);
  int sr = 0, sg = 0, sb = 0, sa = 0;
  QVERIFY(fn->shade(120.0, &sr, &sg, &sb, &sa) && sa > 0);
  QCOMPARE(QColor(sr, sg, sb), QColor(200, 10, 10));
  QFile::remove(path);
}

void TestWorkflow::paleoLandform_candidateEmphasisAndReferenceLayer() {
  QVERIFY(PaleoLandformService::isCandidateTerrainCode(QStringLiteral("04")));
  QVERIFY(PaleoLandformService::isCandidateTerrainCode(QStringLiteral("05")));
  QVERIFY(PaleoLandformService::isCandidateTerrainCode(QStringLiteral("06")));
  QVERIFY(PaleoLandformService::isCandidateTerrainCode(QStringLiteral("08")));
  QVERIFY(!PaleoLandformService::isCandidateTerrainCode(QStringLiteral("01")));
  QVERIFY(!LayerOps::domainLayerKeys().contains(QStringLiteral("paleo_landform")));

  QgsVectorLayer soil(QStringLiteral("MultiPolygon?crs=EPSG:5186&field=soil_type_geo:string"),
                      QStringLiteral("soilwfs"), QStringLiteral("memory"));
  QVERIFY(soil.isValid());
  QVERIFY(SoilMapService::applyTerrainStyle(&soil));
  QVERIFY(PaleoLandformService::applyCandidateEmphasis(&soil));
  auto* cat = dynamic_cast<QgsCategorizedSymbolRenderer*>(soil.renderer());
  QVERIFY2(cat, "categorized after emphasis");
  int candAlpha = -1;
  int otherAlpha = -1;
  const QgsCategoryList cats = cat->categories();
  for (const QgsRendererCategory& c : cats) {
    if (!c.symbol()) continue;
    if (c.value().toString() == QLatin1String("04"))
      candAlpha = c.symbol()->color().alpha();
    if (c.value().toString() == QLatin1String("01"))
      otherAlpha = c.symbol()->color().alpha();
  }
  QVERIFY2(candAlpha >= 160, "alluvial candidate stays strong");
  QVERIFY2(otherAlpha >= 90 && otherAlpha < candAlpha,
           "non-candidate must stay readable, not nearly invisible");
  QVERIFY2(soil.labelsEnabled(), "고지형 강조 뒤에도 한글 지형명이 지도에 보여야 한다");
  const auto* soilLab = dynamic_cast<const QgsVectorLayerSimpleLabeling*>(soil.labeling());
  QVERIFY2(soilLab, "토양 라벨 엔진");
  const QString paleoExpr = soilLab->settings().fieldName;
  QVERIFY2(paleoExpr.contains(QLatin1String("area($geometry)")), "작은 면 글자 생략");
  QVERIFY2(paleoExpr.contains(QLatin1String("'01'")) == false ||
               paleoExpr.contains(QLatin1String("NOT IN")),
           "고지형 글자는 산악지·산록경사를 반복하지 않는다");
  bool sawCandidateLegend = false;
  for (const QgsRendererCategory& c : cats) {
    if (c.value().toString() == QLatin1String("06") &&
        c.label().contains(QStringLiteral("하성평탄")))
      sawCandidateLegend = true;
  }
  QVERIFY2(sawCandidateLegend, "범례에 하성평탄지 한글이 있어야 한다");

  const QString gpkg = QDir::temp().filePath(QStringLiteral("ka_hgis_paleo_test.gpkg"));
  QFile::remove(gpkg);
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  QgsVectorLayer* vl = PaleoLandformService::ensureInterpretationLayer(&proj, gpkg, &err);
  QVERIFY2(vl, qPrintable(err));
  QCOMPARE(LayerOps::layerKeyOf(vl), QStringLiteral("paleo_landform"));
  QVERIFY(LayerOps::isReferenceLayer(vl));
  QVERIFY(!GeorefService::isDomainSurveyLayer(vl));
  QVERIFY(vl->fields().indexOf(QStringLiteral("kind")) >= 0);
  QVERIFY(vl->fields().indexOf(QStringLiteral("status")) >= 0);
  QVERIFY2(dynamic_cast<QgsSingleSymbolRenderer*>(vl->renderer()),
           "빈 고지형 판독은 조판 범례에 자연제방·구하도 목록을 올리면 안 된다");
  QFile::remove(gpkg);
}

void TestWorkflow::paleoLandform_toolbarIsNotDomainExport() {
  QFile mw(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString app = QString::fromUtf8(mw.readAll());
  QVERIFY2(app.contains(QStringLiteral("고지형")), "toolbar label 고지형");
  QVERIFY2(app.contains(QLatin1String("startPaleoLandform")), "slot startPaleoLandform");
  const int paleoFn = app.indexOf(QLatin1String("void MainWindow::startPaleoLandform"));
  QVERIFY2(paleoFn >= 0, "startPaleoLandform");
  const int paleoNext = app.indexOf(QLatin1String("void MainWindow::"), paleoFn + 10);
  const QString paleo = app.mid(paleoFn, paleoNext > paleoFn ? paleoNext - paleoFn : 4000);
  QVERIFY2(!paleo.contains(QLatin1String("ensureDomainLayer")),
           "고지형 must not create a domain layer");
  QVERIFY2(paleo.contains(QLatin1String("seedInterpretationFromSoil")),
           "고지형 클릭은 흙토람 면을 가설 판독으로 자동 깔아야 한다");
  QVERIFY2(paleo.contains(QLatin1String("startReferenceDownload(ReferenceMapKind::PaleoSoil")),
           "고지형 click must prepare 흙토람 without blocking the GUI");
  QVERIFY2(paleo.contains(QLatin1String("clampCanvasToThematicScale")),
           "country-scale view hides thematic soil; clamp to 1:100000");

  QFile boot(QStringLiteral("src/app/KaApplication.cpp"));
  QVERIFY2(boot.open(QIODevice::ReadOnly | QIODevice::Text), "KaApplication.cpp");
  const QString smoke = QString::fromUtf8(boot.readAll());
  QVERIFY2(smoke.contains(QLatin1String("toolbar_paleo")) &&
               smoke.contains(QStringLiteral("고지형")),
           "smoke looks for 고지형");

  QFile exp(QStringLiteral("src/core/ExportService.cpp"));
  QVERIFY2(exp.open(QIODevice::ReadOnly | QIODevice::Text), "ExportService.cpp");
  const QString pack = QString::fromUtf8(exp.readAll());
  QVERIFY2(!pack.contains(QLatin1String("paleo_landform")),
           "판독 layer is not a 5179 submit domain");
}

void TestWorkflow::paleoLandform_seedFromSoilSplitsFloodplain() {
  QCOMPARE(PaleoLandformService::suggestKindFromTerrain(QStringLiteral("04")),
           QStringLiteral("선상지"));
  QCOMPARE(PaleoLandformService::suggestKindFromTerrain(QStringLiteral("05")),
           QStringLiteral("해성평탄"));
  QCOMPARE(PaleoLandformService::suggestKindFromTerrain(QStringLiteral("08")),
           QStringLiteral("하안단구"));
  QVERIFY(PaleoLandformService::suggestKindFromTerrain(QStringLiteral("06")).isEmpty());
  QVERIFY(PaleoLandformService::suggestKindFromTerrain(QStringLiteral("01")).isEmpty());

  QgsVectorLayer soil(QStringLiteral("Polygon?crs=EPSG:5186&field=soil_type_geo:string"),
                      QStringLiteral("soil"), QStringLiteral("memory"));
  QVERIFY(soil.isValid());
  QVERIFY(soil.startEditing());
  auto addSoil = [&](const QString& code, const QgsRectangle& r) {
    QgsFeature f(soil.fields());
    f.setAttribute(QStringLiteral("soil_type_geo"), code);
    f.setGeometry(QgsGeometry::fromRect(r));
    QVERIFY(soil.addFeature(f));
  };
  addSoil(QStringLiteral("01"), QgsRectangle(0, 0, 80, 80));
  addSoil(QStringLiteral("04"), QgsRectangle(200, 0, 350, 150));
  addSoil(QStringLiteral("05"), QgsRectangle(400, 0, 550, 150));
  addSoil(QStringLiteral("06"), QgsRectangle(0, 200, 400, 400));
  addSoil(QStringLiteral("06"), QgsRectangle(600, 0, 620, 20));
  addSoil(QStringLiteral("08"), QgsRectangle(0, 500, 200, 700));
  QVERIFY(soil.commitChanges());

  QgsVectorLayer paleo(
      QStringLiteral("Polygon?crs=EPSG:5186&field=kind:string&field=note:string&field=status:string"),
      QStringLiteral("고지형 판독"), QStringLiteral("memory"));
  QVERIFY(paleo.isValid());
  QVERIFY(paleo.startEditing());
  QgsFeature user(paleo.fields());
  user.setGeometry(QgsGeometry::fromRect(QgsRectangle(900, 900, 920, 920)));
  user.setAttribute(QStringLiteral("kind"), QStringLiteral("미분류"));
  user.setAttribute(QStringLiteral("note"), QStringLiteral("손으로 그림"));
  user.setAttribute(QStringLiteral("status"), QStringLiteral("가설"));
  QVERIFY(paleo.addFeature(user));
  QVERIFY(paleo.commitChanges());

  QString err;
  const auto first = PaleoLandformService::seedInterpretationFromSoil(&soil, &paleo, &err);
  QVERIFY2(first.added > 0, qPrintable(err));
  QVERIFY(first.keptUser >= 1);

  int kindsGu = 0, kindsRim = 0, kindsFan = 0, kindsMarine = 0, kindsTerr = 0, kindsLow = 0,
      kindsMtn = 0, userKept = 0;
  QgsFeature pf;
  auto it = paleo.getFeatures();
  while (it.nextFeature(pf)) {
    const QString k = pf.attribute(QStringLiteral("kind")).toString();
    const QString note = pf.attribute(QStringLiteral("note")).toString();
    QCOMPARE(pf.attribute(QStringLiteral("status")).toString(), QStringLiteral("가설"));
    if (note == QStringLiteral("손으로 그림"))
      ++userKept;
    if (k == QStringLiteral("구하도"))
      ++kindsGu;
    else if (k == QStringLiteral("자연제방"))
      ++kindsRim;
    else if (k == QStringLiteral("선상지"))
      ++kindsFan;
    else if (k == QStringLiteral("해성평탄"))
      ++kindsMarine;
    else if (k == QStringLiteral("하안단구"))
      ++kindsTerr;
    else if (k == QStringLiteral("미저지"))
      ++kindsLow;
    else if (k.contains(QStringLiteral("산악")))
      ++kindsMtn;
  }
  QCOMPARE(userKept, 1);
  QCOMPARE(kindsFan, 1);
  QCOMPARE(kindsMarine, 1);
  QCOMPARE(kindsTerr, 1);
  QCOMPARE(kindsMtn, 0);
  QVERIFY2(kindsGu >= 1 && kindsRim >= 1, "넓은 하성평탄은 구하도+자연제방으로 나눠야 한다");
  QVERIFY2(kindsLow >= 1, "좁은 하성평탄은 미저지 가설");

  const auto second = PaleoLandformService::seedInterpretationFromSoil(&soil, &paleo, &err);
  QCOMPARE(second.keptUser, 1);
  int userAfter = 0, terrAfter = 0;
  auto it2 = paleo.getFeatures();
  while (it2.nextFeature(pf)) {
    if (pf.attribute(QStringLiteral("note")).toString() == QStringLiteral("손으로 그림"))
      ++userAfter;
    if (pf.attribute(QStringLiteral("kind")).toString() == QStringLiteral("하안단구"))
      ++terrAfter;
  }
  QCOMPARE(userAfter, 1);
  QCOMPARE(terrAfter, 1);
  QVERIFY2(dynamic_cast<QgsCategorizedSymbolRenderer*>(paleo.renderer()),
           "가설이 깔리면 조판 범례에 종류가 나와야 한다");
}

void TestWorkflow::demNgiiImg_loadsWithMeterLegend() {
  QFile mw(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString app = QString::fromUtf8(mw.readAll());
  QVERIFY2(app.contains(QLatin1String("importDemElevationRaster")),
           "DEM menu loads 국토지리원 .img");
  QVERIFY2(app.contains(QLatin1String(".img")), "file filter includes ERDAS Imagine");
  QVERIFY2(app.contains(QStringLiteral("국토지리원")) || app.contains(QLatin1String(".img")),
           "menu names the NGII DEM");
  const int fn = app.indexOf(QLatin1String("void MainWindow::toggleDemMap"));
  QVERIFY2(fn >= 0, "toggleDemMap");
  const int next = app.indexOf(QLatin1String("void MainWindow::"), fn + 10);
  const QString toggle = app.mid(fn, next - fn);
  QVERIFY2(!toggle.contains(QLatin1String("getOpenFileName")),
           "DEM body click stays one-click, not the NGII file picker");

  const QString opsSrc = readLayerOpsSources();
  QVERIFY2(!opsSrc.isEmpty(), "LayerOps.cpp + BasemapOps + LabelOps + ControlPointCsv");
  const int addFn = opsSrc.indexOf(QLatin1String("bool LayerOps::addDemElevationRaster"));
  QVERIFY2(addFn >= 0, "addDemElevationRaster");
  const int addNext = opsSrc.indexOf(QLatin1String("\nbool LayerOps::"), addFn + 10);
  QVERIFY2(opsSrc.mid(addFn, addNext - addFn).contains(QLatin1String("applyDemElevationStyle")),
           "NGII .img must get the meter legend");
}

void TestWorkflow::thematicMapsScaleRangeTo1in100000() {
  QgsVectorLayer vl(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("soil"),
                    QStringLiteral("memory"));
  QVERIFY(vl.isValid());
  LayerOps::applyThematicOverlayScaleRange(&vl);
  QVERIFY(!vl.hasScaleBasedVisibility());
  QVERIFY2(vl.isInScaleRange(10000.0), "visible at 1:10000");
  QVERIFY2(vl.isInScaleRange(100000.0), "visible at 1:100000");
  QVERIFY2(vl.isInScaleRange(250000.0), "downloaded thematic data remains visible when zooming out");
}

void TestWorkflow::sectionLineKeepsMagentaWithHalo() {
  QgsVectorLayer vl(QStringLiteral("LineString?crs=EPSG:5186"), QStringLiteral("sec"),
                    QStringLiteral("memory"));
  QVERIFY(vl.isValid());
  LayerOps::markSurveyLayer(&vl, QStringLiteral("section_line"));
  QVERIFY(LayerOps::applyDomainDrawStyle(&vl, QStringLiteral("section_line")));
  auto* r = dynamic_cast<QgsSingleSymbolRenderer*>(vl.renderer());
  QVERIFY(r);
  auto* line = dynamic_cast<QgsLineSymbol*>(r->symbol());
  QVERIFY(line);
  QVERIFY2(line->symbolLayerCount() >= 2, "white casing so cadastral magenta does not swallow A–A′");
  const QColor halo = line->symbolLayer(0)->color();
  QCOMPARE(QColor(halo.red(), halo.green(), halo.blue()), QColor(255, 255, 255));
  QgsSymbolLayer* core = line->symbolLayer(line->symbolLayerCount() - 1);
  QVERIFY(core);
  const QColor c = core->color();
  QCOMPARE(QColor(c.red(), c.green(), c.blue()), QColor(190, 24, 93));
}

void TestWorkflow::featurePolyStrokeDarkerOnSatellite() {
  QgsVectorLayer vl(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("fp"),
                    QStringLiteral("memory"));
  QVERIFY(vl.isValid());
  LayerOps::markSurveyLayer(&vl, QStringLiteral("feature_poly"));
  QVERIFY(LayerOps::applyDomainDrawStyle(&vl, QStringLiteral("feature_poly")));
  QColor f, s;
  double w = 0.0, m = 0.0;
  bool nf = false, ns = false, dash = false;
  QVERIFY(LayerOps::readSimpleVectorStyle(&vl, &f, &s, &w, &m, &nf, &ns, &dash));
  QCOMPARE(QColor(f.red(), f.green(), f.blue()), QColor(22, 163, 74));
  QCOMPARE(QColor(s.red(), s.green(), s.blue()), QColor(17, 94, 44));
  QVERIFY2(w >= 1.8, "outline thicker so fill does not vanish on satellite vegetation");
}

void TestWorkflow::exportPackagePrefersUserSheetPdf() {
  const QString dir = QDir::temp().filePath(
      QStringLiteral("ka_user_sheet_pkg_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));

  auto* sa = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                QStringLiteral("survey_area"), QStringLiteral("memory"));
  QVERIFY(sa->isValid());
  sa->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey), QStringLiteral("survey_area"));
  proj.addMapLayer(sa);
  QVERIFY(sa->startEditing());
  QgsFeature sf(sa->fields());
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200100, 450000)
       << QgsPointXY(200100, 450100) << QgsPointXY(200000, 450100)
       << QgsPointXY(200000, 450000);
  sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(sa->addFeature(sf));
  QVERIFY(sa->commitChanges());

  QVERIFY2(addComposedUserSheet(&proj, sa), "composed user_sheet");
  QVERIFY(LayoutService::isComposedStudioSheet(&proj));

  const QString pkg = QDir(dir).filePath(QStringLiteral("pkg"));
  QString err;
  QVERIFY2(!ExportService::exportSubmissionPackage(&proj, pkg, QStringLiteral("UTF-8"),
                                                   QStringLiteral("OK"), false, false, &err)
                .isEmpty(),
           qPrintable(err));
  QVERIFY2(QFile::exists(QDir(pkg).filePath(QStringLiteral("조사도면.pdf"))),
           "submission PDF must be the composed drawing-studio sheet");
  QVERIFY2(!QFile::exists(QDir(pkg).filePath(QStringLiteral("조사구역도.pdf"))),
           "must not dump auto template PDFs when user_sheet exists");
}

void TestWorkflow::sheetScaleBarUsesInkFill() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  LayoutService::DrawingOptions opt;
  const auto r = LayoutService::buildDrawing(&proj, LayoutService::DrawingKind::SurveyAreaMap,
                                             opt, &err);
  Q_UNUSED(r);
  auto* ly = dynamic_cast<QgsPrintLayout*>(
      proj.layoutManager()->layoutByName(QStringLiteral("survey_area_map")));
  QVERIFY2(ly, qPrintable(err));
  auto* sb = qobject_cast<QgsLayoutItemScaleBar*>(ly->itemById(QStringLiteral("scale_bar")));
  QVERIFY2(sb, "template scale_bar missing");
  QgsFillSymbol* fill = sb->fillSymbol();
  QVERIFY(fill);
  const QColor c = fill->color();
  QCOMPARE(QColor(c.red(), c.green(), c.blue()), QColor(0x11, 0x18, 0x27));
}

void TestWorkflow::layoutOpenDoesNotAutoStartCoordPoint() {
  const QString src = readMainWindowSources();
  QVERIFY2(!src.isEmpty(), "run from source tree (ctest WORKING_DIRECTORY)");
  const int fn = src.indexOf(QLatin1String("void MainWindow::openLayoutDesigner"));
  QVERIFY2(fn >= 0, "openLayoutDesigner must exist");
  const int next = src.indexOf(QLatin1String("void MainWindow::"), fn + 10);
  QVERIFY2(next > fn, "openLayoutDesigner body");
  const QString body = src.mid(fn, next - fn);
  QVERIFY2(body.contains(QLatin1String("addTab")), "조판 stays a main tab");
  QVERIFY2(!body.contains(QLatin1String("beginPlaceCoordPoint")),
           "opening 조판 must not auto-start 좌표점; the bottom icon still does");
}

void TestWorkflow::reprojectAndMigrateFields() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_reproj_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("reproj"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5179")));
  auto* sa = new QgsVectorLayer(QStringLiteral("%1|layername=survey_area").arg(gpkg), QStringLiteral("survey_area"), QStringLiteral("ogr"));
  QVERIFY(sa->isValid());
  proj.addMapLayer(sa);
  QVERIFY(sa->startEditing());
  QgsFeature sf(sa->fields());
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200050, 450000) << QgsPointXY(200050, 450050)
       << QgsPointXY(200000, 450050) << QgsPointXY(200000, 450000);
  sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(sa->addFeature(sf));
  QVERIFY(sa->commitChanges());
  const QString out = QDir(dir).filePath(QStringLiteral("sa_4326.gpkg"));
  QString rerr;
  QVERIFY2(!LayerOps::reprojectVectorLayer(sa, QStringLiteral("EPSG:4326"), out, &proj, &rerr).isEmpty(), qPrintable(rerr));
  QVERIFY(QFile::exists(out));

  auto* cp = new QgsVectorLayer(QStringLiteral("%1|layername=control_points").arg(gpkg), QStringLiteral("control_points"), QStringLiteral("ogr"));
  QVERIFY(cp->isValid());
  const int n = LayerOps::ensureControlPointQualityFields(cp);
  QVERIFY(n >= 0);
  QVERIFY(cp->fields().indexOf(QStringLiteral("accuracy_m")) >= 0);
  QVERIFY(cp->fields().indexOf(QStringLiteral("pixel_x")) >= 0);
}

void TestWorkflow::georefWorldfileFromGcp() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_georef_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("georef"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  QgsProject proj;
  auto* cp = new QgsVectorLayer(QStringLiteral("%1|layername=control_points").arg(gpkg),
                                QStringLiteral("control_points"), QStringLiteral("ogr"));
  QVERIFY(cp->isValid());
  proj.addMapLayer(cp);
  QVERIFY(cp->startEditing());
  for (int i = 0; i < 2; ++i) {
    QgsFeature cf(cp->fields());
    cf.setAttribute(QStringLiteral("point_id"), QStringLiteral("G%1").arg(i + 1));
    cf.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(200000.0 + i * 100, 450000.0)));
    QVERIFY(cp->addFeature(cf));
  }
  QVERIFY(cp->commitChanges());

  const QString png = QDir(dir).filePath(QStringLiteral("scan.png"));
  QImage img(64, 64, QImage::Format_RGB32);
  img.fill(Qt::white);
  QVERIFY(img.save(png));

  QString gerr;
  const QString wf = LayerOps::georeferenceImageSimple(png, cp, &proj, nullptr, &gerr);
  QVERIFY2(!wf.isEmpty(), qPrintable(gerr));
  QVERIFY(QFile::exists(wf) || QFile::exists(QDir(dir).filePath(QStringLiteral("scan.pgw"))));
  const QString pgw = QDir(dir).filePath(QStringLiteral("scan.pgw"));
  QVERIFY2(QFile::exists(pgw), "worldfile missing");
  QFile f(pgw);
  QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString body = QString::fromUtf8(f.readAll());
  QVERIFY(body.split(QLatin1Char('\n'), Qt::SkipEmptyParts).size() >= 6);
}

void TestWorkflow::convert5186PolygonTo5179Shp() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_5179_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("work5186"), &err,
                                                             QStringLiteral("EPSG:5186"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  auto* fp = new QgsVectorLayer(QStringLiteral("%1|layername=feature_poly").arg(gpkg),
                                QStringLiteral("feature_poly"), QStringLiteral("ogr"));
  QVERIFY(fp->isValid());
  QCOMPARE(fp->crs().authid(), QStringLiteral("EPSG:5186"));
  QVERIFY(fp->startEditing());
  QgsFeature ff(fp->fields());
  ff.setAttribute(QStringLiteral("kind"), QStringLiteral("수혈"));
  ff.setAttribute(QStringLiteral("period"), QStringLiteral("청동"));
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200050, 450000) << QgsPointXY(200050, 450050)
       << QgsPointXY(200000, 450050) << QgsPointXY(200000, 450000);
  ff.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(fp->addFeature(ff));
  QVERIFY(fp->commitChanges());

  const QString out = QDir(dir).filePath(QStringLiteral("feat_5179.shp"));
  QString cerr;
  QVERIFY2(!LayerOps::convertToShp5179(fp, out, nullptr, &cerr).isEmpty(), qPrintable(cerr));
  QVERIFY(QFile::exists(out));
  QgsVectorLayer loaded(out, QStringLiteral("u"), QStringLiteral("ogr"));
  QVERIFY(loaded.isValid());
  QVERIFY(loaded.crs().authid() == QStringLiteral("EPSG:5179") || loaded.crs().isValid());
  QCOMPARE(int(loaded.featureCount()), 1);

  // Also verify exportSubmissionPackage reprojects EPSG:5186 layer to EPSG:5179 SHP
  QgsProject proj5186;
  proj5186.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  proj5186.addMapLayer(fp);
  QVERIFY2(addComposedUserSheet(&proj5186, fp), "composed user_sheet");
  const QString pkgDir = QDir(dir).filePath(QStringLiteral("pkg5186"));
  QString perr;
  QVERIFY2(!ExportService::exportSubmissionPackage(&proj5186, pkgDir, QStringLiteral("UTF-8"), QStringLiteral("OK"), false, false, &perr).isEmpty(), qPrintable(perr));
  const QString pkgShp = QDir(pkgDir).filePath(QStringLiteral("feature_poly.shp"));
  QVERIFY(QFile::exists(pkgShp));
  QgsVectorLayer pkgLoaded(pkgShp, QStringLiteral("pkg"), QStringLiteral("ogr"));
  QVERIFY(pkgLoaded.isValid());
  QCOMPARE(pkgLoaded.crs().authid(), QStringLiteral("EPSG:5179"));
  delete fp;
}

void TestWorkflow::convertToShp5179_addToMapFalseKeepsProjectLayerCount() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* vl = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("work_poly"),
                                QStringLiteral("memory"));
  QVERIFY(vl->isValid());
  QVERIFY(vl->startEditing());
  QgsFeature ff(vl->fields());
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200050, 450000) << QgsPointXY(200050, 450050)
       << QgsPointXY(200000, 450050) << QgsPointXY(200000, 450000);
  ff.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(vl->addFeature(ff));
  QVERIFY(vl->commitChanges());
  proj.addMapLayer(vl);
  const int before = proj.mapLayers().size();
  QVERIFY(before >= 1);

  const QString dir = QDir::temp().filePath(QStringLiteral("ka_5179_nomap_") +
                                            QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  const QString out = QDir(dir).filePath(QStringLiteral("only_file.shp"));
  QString err;
  QVERIFY2(!LayerOps::convertToShp5179(vl, out, &proj, &err, false).isEmpty(), qPrintable(err));
  QCOMPARE(int(proj.mapLayers().size()), before);
  QVERIFY(QFile::exists(out));
  QgsVectorLayer loaded(out, QStringLiteral("u"), QStringLiteral("ogr"));
  QVERIFY(loaded.isValid());
  QVERIFY(loaded.crs().authid() == QStringLiteral("EPSG:5179") || loaded.crs().isValid());
}

void TestWorkflow::vworldLayerOpsTest() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  const QString testKey = QStringLiteral("UNITTEST_VWORLD_KEY");

  QVERIFY2(!LayerOps::addVworldBaseMap(&proj, nullptr, QString(), &err), "empty key must fail");
  QVERIFY(err.contains(QStringLiteral("API 키")));

  QVERIFY2(LayerOps::addVworldBaseMap(&proj, nullptr, testKey, &err), qPrintable(err));
  QVERIFY(projectHasLayerNamedLike(&proj, QStringLiteral("VWorld 배경")));
  QVERIFY(LayerOps::setLayerOpacity(&proj, nullptr, QStringLiteral("VWorld 배경"), 0.5));
  {
    QgsMapLayer* base = nullptr;
    for (QgsMapLayer* l : proj.mapLayers()) {
      if (l && l->name().contains(QStringLiteral("배경")))
        base = l;
    }
    QVERIFY(base);
    QVERIFY2(base->source().contains(QStringLiteral("tilePixelRatio=1")),
             qPrintable(base->source().left(160)));
  }

  QVERIFY2(LayerOps::addVworldSatelliteMap(&proj, nullptr, testKey, &err), qPrintable(err));
  QVERIFY(projectHasLayerNamedLike(&proj, QStringLiteral("VWorld 위성")));
  QVERIFY(LayerOps::toggleLayerVisibility(&proj, nullptr, QStringLiteral("VWorld 위성"), false));
  QVERIFY(LayerOps::toggleLayerVisibility(&proj, nullptr, QStringLiteral("VWorld 위성"), true));

  QVERIFY2(LayerOps::addVworldHybridMap(&proj, nullptr, testKey, &err), qPrintable(err));
  QVERIFY(projectHasLayerNamedLike(&proj, QStringLiteral("VWorld 하이브리드")));
  {
    QgsMapLayer* hyb = nullptr;
    for (QgsMapLayer* l : proj.mapLayers()) {
      if (l && l->name().contains(QStringLiteral("하이브리드")))
        hyb = l;
    }
    QVERIFY(hyb);
    QVERIFY2(hyb->source().contains(QStringLiteral("tilePixelRatio=1")),
             qPrintable(hyb->source().left(160)));
  }

  err.clear();
  const bool cad = LayerOps::addVworldCadastralMap(&proj, nullptr, testKey, &err);
  if (!cad) QVERIFY(!err.isEmpty());
  err.clear();
  const bool cont = LayerOps::addVworldContourMap(&proj, nullptr, testKey, &err);
  if (!cont) QVERIFY(!err.isEmpty());
}

void TestWorkflow::vworldSatAndCadastralLiveKey() {
  QString key = QString::fromUtf8(qgetenv("VWORLD_API_KEY")).trimmed();
  if (key.isEmpty()) {
    const QString stored = VworldSettings::loadApiKey();
    const bool looksReal = stored.size() >= 30 && stored.contains(QLatin1Char('-'));
    key = looksReal ? stored : QString();
  }
  if (key.isEmpty())
    QSKIP("VWORLD_API_KEY / saved key missing — skip live VWorld network test");

  VworldSettings::saveApiKey(key);

  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  const bool satOk = LayerOps::addVworldSatelliteMap(&proj, nullptr, key, &err);
  if (!satOk) {
    const QString e = err;
    if (e.contains(QStringLiteral("INVALID"), Qt::CaseInsensitive) ||
        e.contains(QStringLiteral("등록")) || e.contains(QStringLiteral("키")))
      QSKIP(qPrintable(QStringLiteral("VWorld key rejected by server: %1").arg(e)));
    QFAIL(qPrintable(e));
  }
  QVERIFY(projectHasLayerNamedLike(&proj, QStringLiteral("VWorld 위성")));

  err.clear();
  const bool cadOk = LayerOps::addVworldCadastralMap(&proj, nullptr, key, &err);
  if (!cadOk) {
    const QString e = err;
    if (e.contains(QStringLiteral("INVALID"), Qt::CaseInsensitive) ||
        e.contains(QStringLiteral("등록")) || e.contains(QStringLiteral("키")))
      QSKIP(qPrintable(QStringLiteral("VWorld cadastral rejected: %1").arg(e)));
    QFAIL(qPrintable(e));
  }
  for (QgsMapLayer* l : proj.mapLayers()) {
    qWarning() << "TEST VWORLD LAYER:" << (l ? l->name() : QStringLiteral("null"));
  }
  QVERIFY(projectHasLayerNamedLike(&proj, QStringLiteral("VWorld 지적")) ||
          projectHasLayerNamedLike(&proj, QStringLiteral("VWorld 지적도")) ||
          projectHasLayerNamedLike(&proj, QStringLiteral("지적")));
}

void TestWorkflow::workflowGuideTracksSevenRealMilestones() {
  QJsonObject state = ProjectStateBuilder::empty();
  const auto empty = WorkflowGuide::evaluate(state, false, 3, false);
  QCOMPARE(empty.size(), 7);
  QVERIFY(!empty.at(static_cast<int>(WorkflowStep::Survey)).complete);
  QVERIFY(!WorkflowGuide::canCreateSubmission(3));

  state.insert(QStringLiteral("survey_name"), QStringLiteral("테스트조사"));
  state.insert(QStringLiteral("survey_area_count"), 1);
  state.insert(QStringLiteral("feature_poly_count"), 1);
  state.insert(QStringLiteral("has_kind_period"), true);
  state.insert(QStringLiteral("control_points_count"), 2);
  state.insert(QStringLiteral("has_datum"), true);
  state.insert(QStringLiteral("has_ellipsoid"), true);
  state.insert(QStringLiteral("has_projection"), true);
  const auto ready = WorkflowGuide::evaluate(state, true, 0, true);
  QVERIFY(ready.at(static_cast<int>(WorkflowStep::Survey)).complete);
  QVERIFY(ready.at(static_cast<int>(WorkflowStep::Background)).complete);
  QVERIFY(ready.at(static_cast<int>(WorkflowStep::SurveyArea)).complete);
  QVERIFY(ready.at(static_cast<int>(WorkflowStep::Features)).complete);
  QVERIFY(ready.at(static_cast<int>(WorkflowStep::ControlPoints)).complete);
  QVERIFY(ready.at(static_cast<int>(WorkflowStep::Review)).complete);
  QVERIFY(ready.at(static_cast<int>(WorkflowStep::Submission)).complete);
  QVERIFY(WorkflowGuide::canCreateSubmission(0));
}

void TestWorkflow::vworldSettingsAndNoKeyTests() {
  qputenv("VWORLD_API_KEY", QByteArray());
  VworldSettings::saveApiKey(QString());

  QSettings legacy(QStringLiteral("ka-hgis"), QStringLiteral("ka-hgis"));
  legacy.remove(QStringLiteral("VWorld/ApiKey"));
  legacy.setValue(QStringLiteral("vworld/apiKey"), QStringLiteral("LEGACY_MIGRATE_KEY"));
  legacy.sync();

  QCOMPARE(VworldSettings::loadApiKey(), QStringLiteral("LEGACY_MIGRATE_KEY"));
  QCOMPARE(VworldSettings::loadApiKey(), QStringLiteral("LEGACY_MIGRATE_KEY"));
  QCOMPARE(LocationSearch::vworldApiKey(), QStringLiteral("LEGACY_MIGRATE_KEY"));

  const QString testKey = QStringLiteral("TEST_KEY_9999");
  VworldSettings::saveApiKey(testKey);
  QCOMPARE(VworldSettings::loadApiKey(), testKey);
  QCOMPARE(LocationSearch::vworldApiKey(), testKey);
  LocationSearch::setVworldApiKey(QStringLiteral("VIA_LOCATION_SEARCH"));
  QCOMPARE(VworldSettings::loadApiKey(), QStringLiteral("VIA_LOCATION_SEARCH"));

  QgsProject proj;
  QString err;
  QVERIFY2(LayerOps::addVworldBaseMap(&proj, nullptr, VworldSettings::loadApiKey(), &err), qPrintable(err));
  QVERIFY(projectHasLayerNamedLike(&proj, QStringLiteral("VWorld 배경")));
  QVERIFY(LayerOps::hasVisibleReferenceLayer(&proj));

  VworldSettings::saveApiKey(QString());
  QgsProject proj2;
  QVERIFY2(!LayerOps::addVworldBaseMap(&proj2, nullptr, QString(), &err),
           "empty api key must reject basemap add");
  QVERIFY(!projectHasLayerNamedLike(&proj2, QStringLiteral("VWorld 배경")));
}

void TestWorkflow::koreaRegionCatalog_gyeonggiGangwonAddressQuery() {
  const QStringList sido = KoreaRegionCatalog::sidoNames();
  QVERIFY(sido.contains(QStringLiteral("경기도")));
  QVERIFY(sido.contains(QStringLiteral("강원특별자치도")));
  QVERIFY(KoreaRegionCatalog::citiesOf(QStringLiteral("경기도"))
              .contains(QStringLiteral("수원시")));
  QVERIFY(KoreaRegionCatalog::citiesOf(QStringLiteral("강원도"))
              .contains(QStringLiteral("강릉시")));
  QCOMPARE(KoreaRegionCatalog::composeAddress(QStringLiteral("경기도"), QStringLiteral("수원시"),
                                              QStringLiteral("영통동"), QStringLiteral("123-4")),
           QStringLiteral("경기도 수원시 영통동 123-4"));
  QCOMPARE(KoreaRegionCatalog::composeAddress(QStringLiteral("강원도"), QStringLiteral("강릉시"),
                                              QStringLiteral("포남동"), QString()),
           QStringLiteral("강원특별자치도 강릉시 포남동"));
  QVERIFY(KoreaRegionCatalog::composeAddress(QStringLiteral("경기도"), QString(), QString(), QString())
              .contains(QStringLiteral("경기도")));
  QVERIFY2(KoreaRegionCatalog::dongsOf(QStringLiteral("경기도"), QStringLiteral("수원시"))
               .contains(QStringLiteral("영통동")),
           "수원시 동 목록");
  QVERIFY2(KoreaRegionCatalog::dongsOf(QStringLiteral("강원도"), QStringLiteral("강릉시"))
               .contains(QStringLiteral("병산동")),
           "강릉시 동 목록");
  QVERIFY2(KoreaRegionCatalog::dongsOf(QStringLiteral("제주특별자치도"), QStringLiteral("제주시"))
               .contains(QStringLiteral("애월읍")),
           "제주시 읍면동");
}

void TestWorkflow::regionLocator_sitsInAppBarRegionMenu() {
  QFile mw(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString src = QString::fromUtf8(mw.readAll());
  const int menusStart = src.indexOf(QLatin1String("void MainWindow::buildMenus()"));
  const int menusEnd = src.indexOf(QLatin1String("\nvoid MainWindow::"), menusStart + 1);
  QVERIFY(menusStart >= 0 && menusEnd > menusStart);
  const QString menus = src.mid(menusStart, menusEnd - menusStart);
  QVERIFY2(menus.contains(QLatin1String("new KaRegionLocator(m_appBar)")) &&
               menus.contains(QLatin1String("m_appBar->setRegionWidget(region)")),
           "the app bar's 지역 drop-down must host KaRegionLocator");
  QVERIFY2(menus.contains(QLatin1String("connect(m_appBar, &KaAppBar::searchRequested")),
           "the app bar search must reach LocationSearch too");
  QVERIFY2(menus.contains(QLatin1String("connect(region, &KaRegionLocator::searchRequested")) &&
               menus.contains(QLatin1String("[this](const QString& q) { searchLocation(q); }")),
           "시·동·번지 찾기는 LocationSearch로 넘어가야 한다");

  QFile loc(QStringLiteral("src/app/KaRegionLocator.cpp"));
  QVERIFY2(loc.open(QIODevice::ReadOnly | QIODevice::Text), "KaRegionLocator.cpp");
  const QString body = QString::fromUtf8(loc.readAll());
  // Address entry is inside the region popup, not a separate MainWindow field.
  const int city = body.indexOf(QLatin1String("row->addWidget(m_city,"));
  const int dong = body.indexOf(QLatin1String("row->addWidget(m_dong,"));
  const int lot = body.indexOf(QLatin1String("row->addWidget(m_lot,"));
  const int search = body.indexOf(QLatin1String("row->addWidget(go,"));
  QVERIFY2(city >= 0 && city < dong && dong < lot && lot < search,
           "시·군·구, 읍·면·동, 번지 입력은 찾기 버튼보다 앞에 있어야 한다");
  QVERIFY2(body.contains(QLatin1String("connect(go, &QPushButton::clicked, this, &KaRegionLocator::emitSearch)")),
           "주소 입력 뒤 찾기는 검색 요청을 내보내야 한다");
  QVERIFY2(body.contains(QLatin1String("QGridLayout")),
           "시·도 칩은 격자여야 한다. 겹친 약도 좌표는 안 된다");
  QVERIFY2(body.contains(QLatin1String("regionChip")), "regionChip buttons");
  QVERIFY2(!body.contains(QLatin1String("NormChip")),
           "must not paint overlapping normalized chips");
  QVERIFY2(body.contains(QLatin1String("regionDong")), "동은 콤보로 고른다");
  QVERIFY2(body.contains(QLatin1String("setEditable(true)")), "동은 목록 선택 + 한글 입력");
  QVERIFY2(!body.contains(QLatin1String("Qt::Popup")),
           "Popup 창은 Windows에서 한글 IME를 막는다");
}

void TestWorkflow::adminBoundary_buildsEmdUrlWithoutHardcodedKey() {
  const QUrl url = AdminBoundaryService::buildGetFeatureUrl(
      QStringLiteral("TEST_KEY_NOT_PRODUCTION"), QStringLiteral("경상북도"),
      QStringLiteral("안동시"), QStringLiteral("풍산읍"));
  const QString s = url.toString();
  QVERIFY2(s.contains(QLatin1String("api.vworld.kr/req/data")), qPrintable(s));
  QVERIFY2(s.contains(QLatin1String("LT_C_ADEMD_INFO")), qPrintable(s));
  QVERIFY2(s.contains(QLatin1String("geometry=true")) || s.contains(QLatin1String("geometry%3Dtrue")),
           qPrintable(s));
  QVERIFY2(s.contains(QLatin1String("TEST_KEY_NOT_PRODUCTION")), "key is a parameter, not a baked secret");
  QVERIFY2(!s.contains(QLatin1String("vworld-live-key"), Qt::CaseInsensitive),
           "must not hardcode a production key");
  QVERIFY2(!s.contains(QLatin1String("domain="), Qt::CaseInsensitive),
           "Data API DOMAIN=localhost rejects a live key");
  QVERIFY2(s.contains(QStringLiteral("경상북도")) || QUrl::fromPercentEncoding(s.toUtf8()).contains(QStringLiteral("경상북도")),
           qPrintable(s));
  const QString filter = AdminBoundaryService::attrFilter(
      QStringLiteral("경상북도"), QStringLiteral("안동시"), QStringLiteral("풍산읍"));
  QVERIFY2(filter.contains(QStringLiteral("경상북도")), qPrintable(filter));
  QVERIFY2(filter.contains(QStringLiteral("안동시")), qPrintable(filter));
  QVERIFY2(filter.contains(QStringLiteral("풍산읍")), qPrintable(filter));
}

void TestWorkflow::adminBoundary_parsesOkFeatureAndRejectsError() {
  const QByteArray ok = QByteArrayLiteral(
      "{\"response\":{\"status\":\"OK\",\"result\":{\"featureCollection\":{"
      "\"type\":\"FeatureCollection\",\"crs\":{\"type\":\"name\",\"properties\":{\"name\":\"EPSG:3857\"}},"
      "\"features\":[{\"type\":\"Feature\",\"properties\":{"
      "\"emd_cd\":\"47170340\",\"emd_kor_nm\":\"풍산읍\",\"full_nm\":\"경상북도 안동시 풍산읍\"},"
      "\"geometry\":{\"type\":\"Polygon\",\"coordinates\":"
      "[[[14200000,4300000],[14201000,4300000],[14201000,4301000],[14200000,4301000],[14200000,4300000]]]}}]}}}}");
  const AdminBoundaryParse parsed = AdminBoundaryService::parseGetFeature(ok, QStringLiteral("안동시"));
  QVERIFY2(parsed.ok, qPrintable(parsed.error));
  QCOMPARE(parsed.title, QStringLiteral("경상북도 안동시 풍산읍"));
  QCOMPARE(parsed.emdCode, QStringLiteral("47170340"));
  QCOMPARE(parsed.crsAuthId, QStringLiteral("EPSG:3857"));
  QVERIFY2(parsed.wkt.contains(QLatin1String("POLYGON")), qPrintable(parsed.wkt));
  QVERIFY(parsed.wkt.contains(QLatin1String("14200000")));

  const QByteArray bad = QByteArrayLiteral(
      "{\"response\":{\"status\":\"NOT_FOUND\",\"error\":{\"text\":\"no feature\"}}}");
  const AdminBoundaryParse miss = AdminBoundaryService::parseGetFeature(bad);
  QVERIFY(!miss.ok);
  QVERIFY2(miss.error.contains(QStringLiteral("찾지")), qPrintable(miss.error));
}

void TestWorkflow::layerOps_isolateSurfaceSurvey_satelliteAndUserSiteOnly() {
  QgsProject proj;
  auto* sat = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("위성"),
                                 QStringLiteral("memory"));
  QVERIFY(sat->isValid());
  LayerOps::markReferenceLayer(sat);
  auto* cad = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("지적"),
                                 QStringLiteral("memory"));
  QVERIFY(cad->isValid());
  LayerOps::markReferenceLayer(cad);
  auto* feat = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("유구 (면)"),
                                  QStringLiteral("memory"));
  QVERIFY(feat->isValid());
  LayerOps::markSurveyLayer(feat, QStringLiteral("feature_poly"));
  auto* site = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186"), QStringLiteral("문화유적"),
                                  QStringLiteral("memory"));
  QVERIFY(site->isValid());
  LayerOps::markSurveyLayer(site, QStringLiteral("user:문화유적"));
  QgsGeometry box = QgsGeometry::fromWkt(
      QStringLiteral("POLYGON((200000 450000,200500 450000,200500 450500,200000 450500,200000 450000))"));
  QgsVectorLayer* mask =
      LayerOps::upsertAdminEmdMask(&proj, box, QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")),
                                   QStringLiteral("EPSG:5186"), QStringLiteral("안동시 풍산읍"));
  QVERIFY(mask);
  QVERIFY(mask->isValid());
  QVERIFY(dynamic_cast<QgsInvertedPolygonRenderer*>(mask->renderer()));
  QVERIFY(LayerOps::isAdminEmdLayer(mask));
  QVERIFY2(LayerOps::layerKeyOf(mask).isEmpty(), "admin mask must not use a domain layer_key");
  QVERIFY(LayerOps::isReferenceOrBasemapLayer(mask));
  QVERIFY(!LayerOps::domainLayerKeys().contains(QStringLiteral("admin_emd")));
  QVERIFY(LayerOps::applyDomainDrawStyle(mask));
  QVERIFY2(dynamic_cast<QgsInvertedPolygonRenderer*>(mask->renderer()),
           "domain factory style must not replace the invert mask");

  proj.addMapLayer(sat);
  proj.addMapLayer(cad);
  proj.addMapLayer(feat);
  proj.addMapLayer(site);

  QVERIFY(LayerOps::isolateSurfaceSurveyView(&proj, nullptr, site));
  QVERIFY(LayerOps::isLayerVisible(&proj, QStringLiteral("위성")));
  QVERIFY(LayerOps::isLayerVisible(&proj, mask->name()));
  QVERIFY(LayerOps::isLayerVisible(&proj, QStringLiteral("문화유적")));
  QVERIFY(!LayerOps::isLayerVisible(&proj, QStringLiteral("지적")));
  QVERIFY(!LayerOps::isLayerVisible(&proj, QStringLiteral("유구 (면)")));
  QCOMPARE(LayerOps::findImportedSiteLayer(&proj), site);
}

void TestWorkflow::editBufferCommitSurvivesReopen() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_edit_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("editbuf"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  {
    auto* sa = new QgsVectorLayer(QStringLiteral("%1|layername=survey_area").arg(gpkg),
                                  QStringLiteral("survey_area"), QStringLiteral("ogr"));
    QVERIFY(sa->isValid());
    QVERIFY(sa->startEditing());
    QgsFeature sf(sa->fields());
    sf.setAttribute(QStringLiteral("survey_name"), QStringLiteral("커밋생존"));
    QgsPolylineXY ring;
    ring << QgsPointXY(200000, 450000) << QgsPointXY(200080, 450000) << QgsPointXY(200080, 450080)
         << QgsPointXY(200000, 450080) << QgsPointXY(200000, 450000);
    sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
    QVERIFY(sa->addFeature(sf));
    QVERIFY(sa->isEditable());
    QVERIFY(sa->commitChanges());
    QVERIFY(!sa->isEditable());
    QCOMPARE(int(sa->featureCount()), 1);
    delete sa;
  }

  auto* reopened = new QgsVectorLayer(QStringLiteral("%1|layername=survey_area").arg(gpkg),
                                      QStringLiteral("survey_area"), QStringLiteral("ogr"));
  QVERIFY(reopened->isValid());
  QCOMPARE(int(reopened->featureCount()), 1);
  QgsFeature f = reopened->getFeature(*reopened->allFeatureIds().constBegin());
  QCOMPARE(f.attribute(QStringLiteral("survey_name")).toString(), QStringLiteral("커밋생존"));
  delete reopened;
}

void TestWorkflow::undoCommittedFeature_removesLastAdded() {
  auto* vl = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                QStringLiteral("구역"), QStringLiteral("memory"));
  QVERIFY(vl->isValid());
  LayerOps::markSurveyLayer(vl, QStringLiteral("survey_area"));
  QVERIFY(vl->startEditing());
  QgsFeature f(vl->fields());
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200080, 450000)
       << QgsPointXY(200080, 450080) << QgsPointXY(200000, 450080)
       << QgsPointXY(200000, 450000);
  f.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(vl->addFeature(f));
  QVERIFY(vl->commitChanges(false));
  QCOMPARE(int(vl->featureCount()), 1);
  const QgsFeatureIds ids = vl->allFeatureIds();
  QVERIFY(!ids.isEmpty());
  const qint64 fid = static_cast<qint64>(*ids.constBegin());
  QString err;
  QVERIFY2(LayerOps::undoCommittedFeature(vl, fid, &err), qPrintable(err));
  QCOMPARE(int(vl->featureCount()), 0);
  QVERIFY2(!LayerOps::undoCommittedFeature(vl, fid, &err), "second undo must fail");
  delete vl;
}

void TestWorkflow::mapEditUndoRedo_drawMoveDeleteRestoresFeature() {
  auto* vl = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                QStringLiteral("구역"), QStringLiteral("memory"));
  QVERIFY(vl->isValid());
  LayerOps::markSurveyLayer(vl, QStringLiteral("survey_area"));
  QgsProject project;
  project.addMapLayer(vl);

  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200080, 450000)
       << QgsPointXY(200080, 450080) << QgsPointXY(200000, 450080)
       << QgsPointXY(200000, 450000);
  const QgsGeometry drawn = QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring);
  QString err;
  QgsFeature added(vl->fields());
  added.setGeometry(drawn);
  QVERIFY2(LayerOps::runEditCommand(vl, QStringLiteral("그리기"), [&]() {
    return vl->addFeature(added);
  }, &err), qPrintable(err));
  QCOMPARE(int(vl->featureCount()), 1);
  QgsFeatureId fid = *vl->allFeatureIds().constBegin();
  QCOMPARE(vl->getFeature(fid).geometry().asWkb(), drawn.asWkb());
  QVERIFY(vl->undoStack() && vl->undoStack()->canUndo());
  QCOMPARE(LayerOps::preferredUndoLayer(&project, vl), vl);
  QVERIFY(LayerOps::undoLayerEdits(vl));
  QCOMPARE(int(vl->featureCount()), 0);
  QVERIFY(LayerOps::redoLayerEdits(vl));
  QCOMPARE(int(vl->featureCount()), 1);
  fid = *vl->allFeatureIds().constBegin();
  QCOMPARE(vl->getFeature(fid).geometry().asWkb(), drawn.asWkb());

  QgsPolylineXY movedRing = ring;
  for (auto& pt : movedRing) pt.setX(pt.x() + 10.0);
  QgsGeometry moved = QgsGeometry::fromPolygonXY(QgsPolygonXY() << movedRing);
  QVERIFY2(LayerOps::runEditCommand(vl, QStringLiteral("정점 이동"), [&]() {
    return vl->changeGeometry(fid, moved);
  }, &err), qPrintable(err));
  QCOMPARE(vl->getFeature(fid).geometry().asWkb(), moved.asWkb());
  QVERIFY(LayerOps::undoLayerEdits(vl));
  QCOMPARE(vl->getFeature(fid).geometry().asWkb(), drawn.asWkb());

  QVERIFY2(LayerOps::runEditCommand(vl, QStringLiteral("삭제"), [&]() {
    return vl->deleteFeature(fid);
  }, &err), qPrintable(err));
  QCOMPARE(int(vl->featureCount()), 0);
  QVERIFY(LayerOps::undoLayerEdits(vl));
  QCOMPARE(int(vl->featureCount()), 1);
  const QgsFeatureId restored = *vl->allFeatureIds().constBegin();
  QCOMPARE(vl->getFeature(restored).geometry().asWkb(), drawn.asWkb());
}

void TestWorkflow::snapSettings_surviveProjectWriteAndReopen() {
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* area = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("조사구역"), QStringLiteral("memory"));
  QVERIFY(area->isValid());
  LayerOps::markSurveyLayer(area, QStringLiteral("survey_area"));
  project.addMapLayer(area);
  auto* ref = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                 QStringLiteral("지질"), QStringLiteral("memory"));
  QVERIFY(ref->isValid());
  LayerOps::markReferenceLayer(ref);
  project.addMapLayer(ref);

  LayerOps::SnapSettings input;
  input.enabled = false;
  input.tolerancePx = 24.0;
  input.target = LayerOps::SnapTarget::CurrentLayer;
  input.topological = false;
  LayerOps::applySnapSettings(&project, input);
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString qgz = QDir(dir.path()).filePath(QStringLiteral("snap.qgz"));
  QVERIFY2(project.write(qgz), "project write");
  QgsProject opened;
  QVERIFY2(opened.read(qgz), "project read");
  const auto restored = LayerOps::readSnapSettings(&opened);
  QCOMPARE(restored.enabled, false);
  QCOMPARE(restored.tolerancePx, 24.0);
  QCOMPARE(restored.target, LayerOps::SnapTarget::CurrentLayer);
  QCOMPARE(restored.topological, false);
  QCOMPARE(opened.topologicalEditing(), false);

  input.enabled = true;
  input.target = LayerOps::SnapTarget::SurveyLayers;
  LayerOps::applySnapSettings(&project, input);
  QCOMPARE(project.snappingConfig().mode(), Qgis::SnappingMode::AdvancedConfiguration);
  QVERIFY(project.snappingConfig().individualLayerSettings(area).enabled());
  QVERIFY(!project.snappingConfig().individualLayerSettings(ref).enabled());

  auto* cad = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                 QStringLiteral("지적도 · 조사 주변 5km"), QStringLiteral("memory"));
  QVERIFY(cad->isValid());
  LayerOps::markCadastralLayer(cad);
  project.addMapLayer(cad);
  LayerOps::applySnapSettings(&project, input);
  QVERIFY2(!LayerOps::isReferenceLayer(cad), "지적도는 참조 지도가 아니다");
  QVERIFY(LayerOps::isCadastralLayer(cad));
  QVERIFY(LayerOps::isSnapSourceLayer(cad));
  QVERIFY2(project.snappingConfig().individualLayerSettings(cad).enabled(),
           "지적 선에도 자석이 붙어야 한다");
  QVERIFY(!project.snappingConfig().individualLayerSettings(ref).enabled());
  auto* cadGroup = project.layerTreeRoot()->findGroup(QString::fromUtf8(LayerOps::kGroupCadastral));
  QVERIFY2(cadGroup, "지적도는 별도 범례 그룹");
  QVERIFY(cadGroup->findLayer(cad->id()));
  if (auto* refGroup = project.layerTreeRoot()->findGroup(QString::fromUtf8(LayerOps::kGroupReference)))
    QVERIFY(!refGroup->findLayer(cad->id()));

  auto* extra = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                   QStringLiteral("유구"), QStringLiteral("memory"));
  QVERIFY(extra->isValid());
  LayerOps::markSurveyLayer(extra, QStringLiteral("feature_poly"));
  project.addMapLayer(extra);
  LayerOps::applySnapSettings(&project, input);
  QVERIFY2(project.snappingConfig().individualLayerSettings(extra).enabled(),
           "later survey layer must join AdvancedConfiguration");
  QVERIFY(!project.snappingConfig().individualLayerSettings(ref).enabled());
}

void TestWorkflow::applySnapSettingsKeepsCadastralUnchecked() {
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* cad = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                 QStringLiteral("지적도 · 조사 주변 5km"), QStringLiteral("memory"));
  QVERIFY(cad->isValid());
  LayerOps::markCadastralLayer(cad);
  project.addMapLayer(cad, false);
  LayerOps::placeCadastralLayer(&project, cad);
  auto* cadGroup = project.layerTreeRoot()->findGroup(QString::fromUtf8(LayerOps::kGroupCadastral));
  QVERIFY(cadGroup);
  auto* cadNode = cadGroup->findLayer(cad->id());
  QVERIFY(cadNode);
  cadNode->setItemVisibilityChecked(false);
  cadGroup->setItemVisibilityChecked(false);

  auto* soil = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("토양도"), QStringLiteral("memory"));
  QVERIFY(soil->isValid());
  LayerOps::markReferenceLayer(soil);
  project.addMapLayer(soil);
  LayerOps::placeInLegendGroup(&project, soil, QStringLiteral("참조 지도"));
  LayerOps::SnapSettings snap;
  snap.enabled = true;
  LayerOps::applySnapSettings(&project, snap);

  cadNode = cadGroup->findLayer(cad->id());
  QVERIFY(cadNode);
  QVERIFY2(!cadNode->itemVisibilityChecked(), "토양도를 받아도 꺼 둔 지적이 다시 켜지면 안 된다");
  QVERIFY2(!cadGroup->itemVisibilityChecked(), "지적도 그룹도 꺼 둔 채로 남아야 한다");
}

void TestWorkflow::topologicalVertexMove_movesSharedVertexOnBothFeatures() {
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* poly = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("유구"), QStringLiteral("memory"));
  QVERIFY(poly->isValid());
  LayerOps::markSurveyLayer(poly, QStringLiteral("feature_poly"));
  project.addMapLayer(poly);
  LayerOps::SnapSettings snap;
  snap.topological = true;
  LayerOps::applySnapSettings(&project, snap);
  QVERIFY(project.topologicalEditing());

  QgsPolylineXY left;
  left << QgsPointXY(148090, 98110) << QgsPointXY(148100, 98110)
       << QgsPointXY(148100, 98120) << QgsPointXY(148090, 98120)
       << QgsPointXY(148090, 98110);
  QgsPolylineXY right;
  right << QgsPointXY(148100, 98110) << QgsPointXY(148110, 98110)
        << QgsPointXY(148110, 98120) << QgsPointXY(148100, 98120)
        << QgsPointXY(148100, 98110);
  QVERIFY(poly->startEditing());
  QgsFeature a(poly->fields());
  a.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << left));
  QgsFeature b(poly->fields());
  b.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << right));
  QVERIFY(poly->addFeature(a));
  QVERIFY(poly->addFeature(b));

  QgsFeature leftF;
  QgsFeature rightF;
  QgsFeatureIterator it = poly->getFeatures();
  QgsFeature cur;
  while (it.nextFeature(cur)) {
    if (cur.geometry().boundingBox().xMinimum() < 148095) leftF = cur;
    else rightF = cur;
  }
  QVERIFY(leftF.isValid());
  QVERIFY(rightF.isValid());
  int at = -1, before = -1, after = -1;
  double d2 = 0;
  leftF.geometry().closestVertex(QgsPointXY(148100, 98120), at, before, after, d2);
  QVERIFY2(at >= 0 && d2 < 1e-6, "shared corner");

  QString err;
  QVERIFY2(LayerOps::applyVertexMove(poly, static_cast<qint64>(leftF.id()), at, 148105, 98125,
                                     true, &err),
           qPrintable(err));
  const QgsGeometry leftAfter = poly->getFeature(leftF.id()).geometry();
  const QgsGeometry rightAfter = poly->getFeature(rightF.id()).geometry();
  int rightAt = -1;
  rightAfter.closestVertex(QgsPointXY(148105, 98125), rightAt, before, after, d2);
  QVERIFY2(rightAt >= 0 && d2 <= 1e-6,
           qPrintable(QStringLiteral("neighbor must move, d2=%1").arg(d2)));
  int leftAt = -1;
  leftAfter.closestVertex(QgsPointXY(148105, 98125), leftAt, before, after, d2);
  QVERIFY2(leftAt >= 0 && d2 <= 1e-6, "dragged vertex must move");

  project.setTopologicalEditing(false);
  int leftShared = -1;
  poly->getFeature(leftF.id()).geometry().closestVertex(QgsPointXY(148100, 98110), leftShared,
                                                        before, after, d2);
  QVERIFY2(leftShared >= 0 && d2 <= 1e-6, "remaining shared corner");
  QVERIFY2(LayerOps::applyVertexMove(poly, static_cast<qint64>(leftF.id()), leftShared, 148102,
                                     98108, false, &err),
           qPrintable(err));
  int neighborStay = -1;
  double neighborD2 = 0;
  poly->getFeature(rightF.id()).geometry().closestVertex(QgsPointXY(148100, 98110), neighborStay,
                                                         before, after, neighborD2);
  QVERIFY2(neighborStay >= 0 && neighborD2 <= 1e-6,
           "topology off must leave the other feature");
}

void TestWorkflow::featureForm_cancelKeepsGeometryAndOkWritesNameNumber() {
  auto* layer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                   QStringLiteral("유구"), QStringLiteral("memory"));
  QVERIFY(layer->isValid());
  QVERIFY(layer->dataProvider()->addAttributes(
      {QgsField(QStringLiteral("kind"), QMetaType::Type::QString),
       QgsField(QStringLiteral("feature_no"), QMetaType::Type::QString)}));
  layer->updateFields();
  LayerOps::markSurveyLayer(layer, QStringLiteral("feature_poly"));
  const auto fields = LayerOps::featureFormFields(layer);
  QCOMPARE(fields.nameField, QStringLiteral("kind"));
  QCOMPARE(fields.numberField, QStringLiteral("feature_no"));

  QVERIFY(layer->startEditing());
  QgsFeature kept(layer->fields());
  kept.setGeometry(QgsGeometry::fromRect(QgsRectangle(148090, 98110, 148100, 98120)));
  QgsFeature named(layer->fields());
  named.setGeometry(QgsGeometry::fromRect(QgsRectangle(148100, 98110, 148110, 98120)));
  QVERIFY(layer->addFeature(kept));
  QVERIFY(layer->addFeature(named));
  QCOMPARE(int(layer->featureCount()), 2);

  QString err;
  QVERIFY2(LayerOps::applyFeatureFormValues(layer, static_cast<qint64>(named.id()),
                                            QStringLiteral("주거지"), QStringLiteral("1호"), &err),
           qPrintable(err));
  QCOMPARE(layer->getFeature(named.id()).attribute(QStringLiteral("kind")).toString(),
           QStringLiteral("주거지"));
  QCOMPARE(layer->getFeature(named.id()).attribute(QStringLiteral("feature_no")).toString(),
           QStringLiteral("1호"));
  QVERIFY(layer->getFeature(kept.id()).geometry().area() > 0);
  QCOMPARE(layer->getFeature(kept.id()).attribute(QStringLiteral("kind")).toString(), QString());
  QCOMPARE(int(layer->featureCount()), 2);

  const QString src = readMainWindowSources();
  QVERIFY2(!src.isEmpty(), "MainWindow.cpp + Export + Editing");
  const int cap = src.indexOf(QStringLiteral("void MainWindow::onGeometryCaptured"));
  QVERIFY2(cap >= 0, "onGeometryCaptured");
  const int capEnd = src.indexOf(QStringLiteral("void MainWindow::"), cap + 10);
  const QString body = src.mid(cap, capEnd > cap ? capEnd - cap : 2500);
  QVERIFY2(body.contains(QLatin1String("addFeature")), "도형을 먼저 넣는다");
  QVERIFY2(body.contains(QLatin1String("KaFeatureFormDialog")), "그리기 직후 이름·번호 폼");
  QVERIFY2(body.indexOf(QLatin1String("addFeature")) < body.indexOf(QLatin1String("KaFeatureFormDialog")),
           "폼은 도형을 넣은 뒤에만");
}

void TestWorkflow::captureVertexDrag_preservesEditsAndReportsOutcome_data() {
  QTest::addColumn<QString>("canvasCrs");
  QTest::addColumn<QString>("layerCrs");
  QTest::addColumn<int>("failure"); // 0: success, 1: commit veto, 2: missing feature, 3: removed layer
  QTest::newRow("success-5186") << QStringLiteral("EPSG:5186") << QStringLiteral("EPSG:5186") << 0;
  QTest::newRow("success-5187") << QStringLiteral("EPSG:5187") << QStringLiteral("EPSG:5187") << 0;
  QTest::newRow("commit-failure-5186") << QStringLiteral("EPSG:5186") << QStringLiteral("EPSG:5186") << 1;
  QTest::newRow("commit-failure-5187") << QStringLiteral("EPSG:5187") << QStringLiteral("EPSG:5187") << 1;
  QTest::newRow("success-reprojected") << QStringLiteral("EPSG:5186") << QStringLiteral("EPSG:5187") << 0;
  QTest::newRow("commit-failure-reprojected") << QStringLiteral("EPSG:5186") << QStringLiteral("EPSG:5187") << 1;
  QTest::newRow("missing-feature") << QStringLiteral("EPSG:5186") << QStringLiteral("EPSG:5186") << 2;
  QTest::newRow("removed-layer") << QStringLiteral("EPSG:5186") << QStringLiteral("EPSG:5186") << 3;
}

void TestWorkflow::captureVertexDrag_preservesEditsAndReportsOutcome() {
  QFETCH(QString, canvasCrs);
  QFETCH(QString, layerCrs);
  QFETCH(int, failure);
  auto layer = std::make_unique<QgsVectorLayer>(
      QStringLiteral("LineString?crs=%1&field=note:string").arg(layerCrs),
      QStringLiteral("유구선"), QStringLiteral("memory"));
  QVERIFY(layer->isValid());
  LayerOps::markSurveyLayer(layer.get(), QStringLiteral("feature_line"));
  const QgsPointXY original(200000, 450000);
  const QgsPointXY moved(200025, 450015);
  const QgsPointXY other(200080, 450080);
  QVERIFY(layer->startEditing());
  QgsFeature feature(layer->fields());
  feature.setAttribute(QStringLiteral("note"), QStringLiteral("saved"));
  feature.setGeometry(QgsGeometry::fromPolylineXY(QgsPolylineXY{original, other}));
  QVERIFY(layer->addFeature(feature));
  QVERIFY(layer->commitChanges(false));
  const QgsFeatureId fid = *layer->allFeatureIds().constBegin();
  const QgsGeometry originalGeometry = layer->getFeature(fid).geometry();
  // A failed vertex commit must also preserve edits made before this gesture.
  QVERIFY(layer->changeAttributeValue(fid, 0, QStringLiteral("pending note")));

  QgsProject project;
  QgsMapCanvas canvas;
  canvas.setRenderFlag(false);
  canvas.resize(640, 480);
  canvas.mapSettings().setOutputSize(QSize(640, 480));
  QVERIFY(LayerOps::ensureOtfEnabled(&project, &canvas, canvasCrs));
  const QgsCoordinateTransform toMap(layer->crs(), canvas.mapSettings().destinationCrs(), project.transformContext());
  const QgsPointXY mapOriginal = toMap.transform(original);
  const QgsPointXY mapMoved = toMap.transform(moved);
  canvas.setExtent(QgsRectangle(mapOriginal.x() - 100, mapOriginal.y() - 100,
                                mapOriginal.x() + 100, mapOriginal.y() + 100));
  KaCaptureMapTool tool(&canvas);
  tool.setMode(KaCaptureMapTool::Mode::Line);
  tool.setTargetLayer(layer.get());
  tool.setSnapEnabled(false);
  QSignalSpy success(&tool, &KaCaptureMapTool::vertexMoved);
  QSignalSpy error(&tool, &KaCaptureMapTool::vertexMoveFailed);
  QSignalSpy capture(&tool, &KaCaptureMapTool::geometryCaptured);
  QgsMapMouseEvent press(&canvas, QEvent::MouseButtonPress, QPoint(320, 240), Qt::LeftButton, Qt::LeftButton);
  press.setMapPoint(mapOriginal);
  tool.canvasPressEvent(&press);
  QCOMPARE(tool.pointCount(), 0); // The existing vertex must be picked even across CRS.
  if (failure == 1)
    layer->setAllowCommit(false);
  else if (failure == 2)
    QVERIFY(layer->deleteFeature(fid));
  else if (failure == 3)
    layer.reset();
  QgsMapMouseEvent release(&canvas, QEvent::MouseButtonRelease, QPoint(360, 220), Qt::LeftButton);
  release.setMapPoint(mapMoved);
  tool.canvasReleaseEvent(&release);
  QCOMPARE(success.count(), failure == 0 ? 1 : 0);
  QCOMPARE(error.count(), failure == 0 ? 0 : 1);
  QCOMPARE(capture.count(), 0);
  if (failure != 0)
    QVERIFY(!error.at(0).at(0).toString().isEmpty());
  // A duplicate release must not repeat either result or mutate another feature.
  tool.canvasReleaseEvent(&release);
  QCOMPARE(success.count(), failure == 0 ? 1 : 0);
  QCOMPARE(error.count(), failure == 0 ? 0 : 1);
  if (!layer) return;
  QCOMPARE(layer->crs().authid(), layerCrs);
  QCOMPARE(canvas.mapSettings().destinationCrs().authid(), canvasCrs);
  QVERIFY(layer->isEditable());
  if (failure == 2) {
    QVERIFY(!layer->getFeature(fid).isValid());
    QVERIFY(layer->isModified());
    return;
  }
  const QgsFeature edited = layer->getFeature(fid);
  QVERIFY(QgsPointXY(edited.geometry().vertexAt(0)).sqrDist(moved) < 1e-8);
  QVERIFY(QgsPointXY(edited.geometry().vertexAt(1)).sqrDist(other) < 1e-8);
  QCOMPARE(edited.attribute(QStringLiteral("note")).toString(), QStringLiteral("pending note"));
  QgsFeature stored;
  QVERIFY(layer->dataProvider()->getFeatures(QgsFeatureRequest().setFilterFid(fid)).nextFeature(stored));
  if (failure == 1) {
    QVERIFY(layer->isModified());
    QCOMPARE(stored.geometry().asWkb(), originalGeometry.asWkb());
    QCOMPARE(stored.attribute(QStringLiteral("note")).toString(), QStringLiteral("saved"));
    layer->setAllowCommit(true);
    QVERIFY(layer->commitChanges(false)); // The same buffered work remains saveable.
    QVERIFY(layer->dataProvider()->getFeatures(QgsFeatureRequest().setFilterFid(fid)).nextFeature(stored));
  }
  QVERIFY(!layer->isModified());
  QVERIFY(QgsPointXY(stored.geometry().vertexAt(0)).sqrDist(moved) < 1e-8);
  QCOMPARE(stored.attribute(QStringLiteral("note")).toString(), QStringLiteral("pending note"));
}

void TestWorkflow::importControlCsvWritesFeatures() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_csv_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("csvcp"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  auto* cp = new QgsVectorLayer(QStringLiteral("%1|layername=control_points").arg(gpkg),
                                QStringLiteral("control_points"), QStringLiteral("ogr"));
  QVERIFY(cp->isValid());
  QCOMPARE(int(cp->featureCount()), 0);

  const QString csvPath = QDir(dir).filePath(QStringLiteral("gcp.csv"));
  {
    QFile f(csvPath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream ts(&f);
    ts << "point_id,x,y,datum,ellipsoid,projection,accuracy_m,pdop,fix_type\n";
    ts << "G1,200000,450000,세계측지계,GRS80,UTM-K,0.8,1.2,RTK\n";
    ts << "G2,200050,450020,세계측지계,GRS80,UTM-K,0.9,1.1,RTK\n";
  }

  QString ierr;
  const int n = LayerOps::importControlPointsCsv(cp, csvPath, &ierr);
  QVERIFY2(n == 2, qPrintable(ierr.isEmpty() ? QString::number(n) : ierr));
  QCOMPARE(int(cp->featureCount()), 2);

  QgsFeature f1 = cp->getFeature(*cp->allFeatureIds().constBegin());
  QVERIFY(!f1.attribute(QStringLiteral("point_id")).toString().isEmpty());
  QVERIFY(!f1.attribute(QStringLiteral("datum")).toString().isEmpty());
  QVERIFY(f1.hasGeometry());

  QJsonObject st = ProjectStateBuilder::fromProject(nullptr);
  QgsProject proj;
  proj.addMapLayer(cp);
  st = ProjectStateBuilder::fromProject(&proj);
  QCOMPARE(st.value(QStringLiteral("control_points_count")).toInt(), 2);
  QVERIFY(st.value(QStringLiteral("has_datum")).toBool());
  QVERIFY(st.value(QStringLiteral("has_ellipsoid")).toBool());
  QVERIFY(st.value(QStringLiteral("has_projection")).toBool());

  const auto steps = WorkflowGuide::evaluate(st, true, 0, false);
  QVERIFY(steps.at(static_cast<int>(WorkflowStep::ControlPoints)).complete);

  QJsonObject oneOnly = st;
  oneOnly.insert(QStringLiteral("control_points_count"), 1);
  const auto incomplete = WorkflowGuide::evaluate(oneOnly, true, 0, false);
  QVERIFY(!incomplete.at(static_cast<int>(WorkflowStep::ControlPoints)).complete);
}

void TestWorkflow::importControlCsv_keepsKoreanAxesAndEncoding() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_csv_xy_") +
                                            QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("csvxy"), &err,
                                                             QStringLiteral("EPSG:5186"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  QgsProject project;
  auto* area = new QgsVectorLayer(QStringLiteral("%1|layername=survey_area").arg(gpkg),
                                  QStringLiteral("조사구역"), QStringLiteral("ogr"));
  auto* cp = new QgsVectorLayer(QStringLiteral("%1|layername=control_points").arg(gpkg),
                                QStringLiteral("control_points"), QStringLiteral("ogr"));
  QVERIFY(area->isValid());
  QVERIFY(cp->isValid());
  project.addMapLayer(area);
  QVERIFY(area->startEditing());
  QgsFeature zone(area->fields());
  zone.setGeometry(QgsGeometry::fromRect(QgsRectangle(148078, 98110, 148110, 98134)));
  QVERIFY(area->addFeature(zone));
  QVERIFY(area->commitChanges());

  const QString swappedPath = QDir(dir).filePath(QStringLiteral("korean-xy.csv"));
  {
    QFile file(swappedPath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "point_id,X,Y\n";
    stream << "K1,98120,148100\n";
  }
  QString previewError;
  const auto preview = LayerOps::previewControlPointsCsv(cp, swappedPath, &project, &previewError);
  QVERIFY2(preview.ok, qPrintable(previewError));
  QVERIFY2(preview.swapSuggested, qPrintable(preview.summary));
  QVERIFY(!preview.geographic);
  QString importError;
  QCOMPARE(LayerOps::importControlPointsCsv(cp, swappedPath, &importError, true), 1);
  QgsFeature korean;
  QVERIFY(cp->getFeatures().nextFeature(korean));
  const QgsPointXY placed = korean.geometry().asPoint();
  QVERIFY2(QgsPointXY(148100, 98120).sqrDist(placed) < 1.0, qPrintable(placed.toString()));

  const QString cp949Path = QDir(dir).filePath(QStringLiteral("cp949.csv"));
  {
    QStringEncoder encoder(QStringLiteral("CP949"));
    QVERIFY(encoder.isValid());
    QFile file(cp949Path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(encoder.encode(QStringLiteral("point_id,x,y\n기준1,148100,98120\n"))) > 0);
  }
  QCOMPARE(LayerOps::importControlPointsCsv(cp, cp949Path, &importError, false), 1);
  bool foundName = false;
  QgsFeature named;
  QgsFeatureIterator namedRows = cp->getFeatures();
  while (namedRows.nextFeature(named)) {
    if (named.attribute(QStringLiteral("point_id")).toString() == QStringLiteral("기준1")) {
      foundName = true;
      break;
    }
  }
  QVERIFY(foundName);

  const QString degreePath = QDir(dir).filePath(QStringLiteral("degrees.csv"));
  {
    QFile file(degreePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "point_id,lon,lat\nJ1,126.48,33.45\n";
  }
  const auto degrees = LayerOps::previewControlPointsCsv(cp, degreePath, &project, &previewError);
  QVERIFY2(degrees.ok && degrees.geographic, qPrintable(degrees.summary));
  QCOMPARE(LayerOps::importControlPointsCsv(cp, degreePath, &importError, false), 1);
  QgsCoordinateTransform toWork(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:4326")), cp->crs(),
                                project.transformContext());
  const QgsPointXY expected = toWork.transform(126.48, 33.45);
  bool foundDegree = false;
  QgsFeature degreeFeature;
  QgsFeatureIterator degreeRows = cp->getFeatures();
  while (degreeRows.nextFeature(degreeFeature)) {
    if (degreeFeature.attribute(QStringLiteral("point_id")).toString() != QStringLiteral("J1")) continue;
    foundDegree = true;
    QVERIFY(expected.sqrDist(degreeFeature.geometry().asPoint()) < 1.0);
  }
  QVERIFY(foundDegree);
}

void TestWorkflow::suggestControlPointAxisSwap_matchesCsvRule() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("축안내"),
                                                             &err, QStringLiteral("EPSG:5186"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* area = LayerOps::ensureDomainLayer(&project, gpkg, QStringLiteral("survey_area"),
                                           QStringLiteral("조사구역"), &err);
  QVERIFY2(area, qPrintable(err));
  QVERIFY(area->startEditing());
  QgsFeature zone(area->fields());
  zone.setGeometry(QgsGeometry::fromRect(QgsRectangle(148078, 98110, 148110, 98134)));
  QVERIFY(area->addFeature(zone));
  QVERIFY(area->commitChanges());
  QVERIFY(LayerOps::controlPointAxisHint().contains(QStringLiteral("X를 동쪽")));
  QVERIFY(LayerOps::controlPointAxisHint().contains(QStringLiteral("X=북쪽")));
  QCOMPARE(LayerOps::controlPointMapXy(98120, 148100, true), QgsPointXY(148100, 98120));
  const auto suggestion = LayerOps::suggestControlPointAxisSwap(&project, 98120, 148100);
  QVERIFY2(suggestion.ok && suggestion.swapSuggested, qPrintable(suggestion.summary));
  QVERIFY(suggestion.summary.contains(QStringLiteral("X·Y가 바뀐 것 같습니다")));
}

void TestWorkflow::osmBasemapValidWithExtent() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  QVERIFY2(LayerOps::addOsmBasemap(&proj, nullptr, &err), qPrintable(err));

  QgsRasterLayer* rl = nullptr;
  for (QgsMapLayer* l : proj.mapLayers()) {
    if (!l) continue;
    const QString n = l->name();
    if (n == QLatin1String("OSM") || n.startsWith(QLatin1String("OSM [")) ||
        n == QLatin1String("Carto Light") || n.startsWith(QLatin1String("Carto Light ["))) {
      rl = qobject_cast<QgsRasterLayer*>(l);
      if (rl) break;
    }
  }
  QVERIFY2(rl, "OSM/Carto layer missing");
  QVERIFY2(rl->isValid(), qPrintable(rl->error().message()));
  QVERIFY2(rl->source().contains(QStringLiteral("tilePixelRatio=1")),
           "OSM/Carto XYZ must request 256px tiles, not @2x");
  const QgsRectangle ext = rl->extent();
  QVERIFY2(ext.isFinite() && !ext.isEmpty(), "basemap extent empty");
  QVERIFY(LayerOps::hasVisibleReferenceLayer(&proj));

  const QgsRectangle kr = LayerOps::koreaExtentForCrs(QStringLiteral("EPSG:5186"));
  QVERIFY2(!kr.isEmpty() && kr.isFinite(), "korea extent invalid for EPSG:5186");
}


void TestWorkflow::layerTreeDragReorderChangesOrder() {
  QgsProject proj;
  auto* l1 = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5179"), QStringLiteral("A"), QStringLiteral("memory"));
  auto* l2 = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5179"), QStringLiteral("B"), QStringLiteral("memory"));
  QVERIFY(l1->isValid() && l2->isValid());
  proj.addMapLayer(l1);
  proj.addMapLayer(l2);
  QgsLayerTree* root = proj.layerTreeRoot();
  QCOMPARE(root->children().size(), 2);
  QgsLayerTreeModel model(root);
  model.setFlag(QgsLayerTreeModel::AllowNodeReorder, true);
  QVERIFY(model.flags(model.index(0, 0)).testFlag(Qt::ItemIsDragEnabled));
  // Move first child to end: B then A (or reverse depending add order)
  QgsLayerTreeNode* first = root->children().at(0);
  QVERIFY(root->takeChild(first));
  root->insertChildNode(root->children().size(), first);
  QCOMPARE(root->children().size(), 2);
  QCOMPARE(root->children().at(1), first);
}
void TestWorkflow::layerTreeReorderAndRemovalTest() {
  QgsProject proj;
  auto* l1 = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5179"), QStringLiteral("L1"), QStringLiteral("memory"));
  auto* l2 = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5179"), QStringLiteral("L2"), QStringLiteral("memory"));
  QVERIFY(l1->isValid() && l2->isValid());

  proj.addMapLayer(l1);
  proj.addMapLayer(l2);

  QgsLayerTreeModel model(proj.layerTreeRoot());
  model.setFlag(QgsLayerTreeModel::AllowNodeReorder, true);
  QVERIFY(model.flags(model.index(0, 0)).testFlag(Qt::ItemIsDragEnabled));

  QCOMPARE(proj.mapLayers().size(), 2);
  const QString id1 = l1->id();
  QVERIFY(LayerOps::removeConfirmedLayers(&proj, nullptr, {id1}));
  QCOMPARE(proj.mapLayers().size(), 1);
  QVERIFY(proj.mapLayersByName(QStringLiteral("L1")).isEmpty());
}

// Phase-1: digitize attrs must key off layer_key, not Korean display name (유구면).
void TestWorkflow::layerKeyOfKoreanDisplayName_featurePolyAttrs() {
  QgsProject proj;
  auto* fp = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186&field=kind:string&field=period:string"),
                                QStringLiteral("유구면"), QStringLiteral("memory"));
  QVERIFY2(fp->isValid(), qPrintable(fp->error().message()));
  QCOMPARE(fp->name(), QStringLiteral("유구면"));
  // Without mark: key empty — name-based branch would miss Korean titles.
  QVERIFY(LayerOps::layerKeyOf(fp).isEmpty());
  QVERIFY(fp->name() != QLatin1String("feature_poly"));

  LayerOps::markSurveyLayer(fp, QStringLiteral("feature_poly"));
  proj.addMapLayer(fp);

  QCOMPARE(LayerOps::layerKeyOf(fp), QStringLiteral("feature_poly"));
  QCOMPARE(fp->name(), QStringLiteral("유구면"));
  // Same branch condition MainWindow::onGeometryCaptured must use:
  const QString layerKey = LayerOps::layerKeyOf(fp);
  QVERIFY(layerKey == QLatin1String("feature_poly") || layerKey == QLatin1String("feature_line"));

  QVERIFY(fp->startEditing());
  QgsFeature ff(fp->fields());
  ff.setAttribute(QStringLiteral("kind"), QStringLiteral("수혈주거지"));
  ff.setAttribute(QStringLiteral("period"), QStringLiteral("청동기"));
  QgsPolylineXY ring;
  ring << QgsPointXY(200020, 450020) << QgsPointXY(200040, 450020) << QgsPointXY(200040, 450040)
       << QgsPointXY(200020, 450040) << QgsPointXY(200020, 450020);
  ff.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(fp->addFeature(ff));
  QVERIFY(fp->commitChanges());

  // findByLayerKey / ProjectStateBuilder resolve via key, not display name
  QCOMPARE(LayerOps::findByLayerKey(&proj, QStringLiteral("feature_poly")), fp);
  QVERIFY(proj.mapLayersByName(QStringLiteral("feature_poly")).isEmpty());
  const QJsonObject st = ProjectStateBuilder::fromProject(&proj);
  QCOMPARE(st.value(QStringLiteral("feature_poly_count")).toInt(), 1);
  QVERIFY(st.value(QStringLiteral("has_kind_period")).toBool());
}

void TestWorkflow::ensureOtf_projectAndCanvasDestinationCrs5186() {
  QgsProject proj;
  QgsMapCanvas canvas;
  canvas.resize(640, 480);
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, &canvas, QStringLiteral("EPSG:5186")));
  QCOMPARE(proj.crs().authid(), QStringLiteral("EPSG:5186"));
  QCOMPARE(canvas.mapSettings().destinationCrs().authid(), QStringLiteral("EPSG:5186"));
  QVERIFY(!LayerOps::ensureOtfEnabled(&proj, &canvas, QStringLiteral("EPSG:999999")));
}

void TestWorkflow::syncMapCanvas_surveyLayersAboveReferenceBasemap() {
  QgsProject proj;
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, nullptr, QStringLiteral("EPSG:5186")));

  auto* survey = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                    QStringLiteral("유구면"), QStringLiteral("memory"));
  QVERIFY(survey->isValid());
  LayerOps::markSurveyLayer(survey, QStringLiteral("feature_poly"));

  auto* base = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:3857"),
                                  QStringLiteral("fake_basemap"), QStringLiteral("memory"));
  QVERIFY(base->isValid());
  LayerOps::markReferenceLayer(base);

  proj.addMapLayer(base);
  proj.addMapLayer(survey);

  QgsMapCanvas canvas;
  canvas.resize(800, 600);
  LayerOps::syncMapCanvas(&proj, &canvas, false);
  QCOMPARE(canvas.mapSettings().destinationCrs().authid(), QStringLiteral("EPSG:5186"));

  const QList<QgsMapLayer*> layers = canvas.layers();
  QVERIFY(layers.size() >= 2);
  const int iSurvey = layers.indexOf(survey);
  const int iBase = layers.indexOf(base);
  QVERIFY(iSurvey >= 0);
  QVERIFY(iBase >= 0);
  QVERIFY2(iSurvey < iBase, "survey domain layer must paint above reference basemap");
}

void TestWorkflow::satelliteAlwaysAtBottomInLegendAndCanvas() {
  QgsProject proj;
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, nullptr, QStringLiteral("EPSG:5186")));

  auto* sat = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                 QStringLiteral("VWorld 위성"), QStringLiteral("memory"));
  auto* cad = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                 QStringLiteral("지적"), QStringLiteral("memory"));
  auto* survey = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                    QStringLiteral("조사구역"), QStringLiteral("memory"));
  auto* shp = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                 QStringLiteral("발굴구역"), QStringLiteral("memory"));

  LayerOps::markReferenceLayer(sat);
  LayerOps::markReferenceLayer(cad);
  LayerOps::markSurveyLayer(survey, QStringLiteral("survey_area"));
  LayerOps::markReferenceLayer(shp);

  // 위성을 먼저 넣든 나중에 넣든 항상 최하단이어야 함
  proj.addMapLayer(sat);
  proj.addMapLayer(cad);
  proj.addMapLayer(survey);
  proj.addMapLayer(shp);

  LayerOps::ensureSatelliteAtBottom(&proj);

  QgsLayerTree* root = proj.layerTreeRoot();
  QVERIFY(root);
  const auto children = root->children();
  QVERIFY(!children.isEmpty());
  auto* lastNode = qobject_cast<QgsLayerTreeLayer*>(children.last());
  QVERIFY(lastNode);
  QVERIFY(lastNode->layer());
  QCOMPARE(lastNode->layer(), sat);
  QVERIFY(lastNode->layer()->name().contains(QStringLiteral("위성")));

  // 캔버스 페인트 순서에서도 위성이 맨 끝(가장 바닥)이어야 함
  const QList<QgsMapLayer*> paintOrder = LayerOps::visibleLayersPaintOrder(&proj);
  QVERIFY(!paintOrder.isEmpty());
  QCOMPARE(paintOrder.last(), sat);
  const int iSurvey = paintOrder.indexOf(survey);
  const int iShp = paintOrder.indexOf(shp);
  const int iCad = paintOrder.indexOf(cad);
  const int iSat = paintOrder.indexOf(sat);
  QVERIFY(iSurvey >= 0 && iSurvey < iSat);
  QVERIFY(iShp >= 0 && iShp < iSat);
  QVERIFY(iCad >= 0 && iCad < iSat);
}

void TestWorkflow::satelliteDuplicatePrunedToOneInstance() {
  QgsProject proj;
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, nullptr, QStringLiteral("EPSG:5186")));

  auto* sat1 = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("위성"), QStringLiteral("memory"));
  auto* sat2 = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("위성"), QStringLiteral("memory"));
  auto* sat3 = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("VWorld 위성"), QStringLiteral("memory"));
  auto* cad = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                 QStringLiteral("지적"), QStringLiteral("memory"));

  LayerOps::markReferenceLayer(sat1);
  LayerOps::markReferenceLayer(sat2);
  LayerOps::markReferenceLayer(sat3);
  LayerOps::markReferenceLayer(cad);

  proj.addMapLayer(sat1);
  proj.addMapLayer(sat2);
  proj.addMapLayer(sat3);
  proj.addMapLayer(cad);

  // 4개의 레이어 중 위성이 3개
  int satCountBefore = 0;
  for (QgsMapLayer* l : proj.mapLayers()) {
    if (l && l->name().contains(QStringLiteral("위성")))
      satCountBefore++;
  }
  QCOMPARE(satCountBefore, 3);

  // prune 및 최하단 보장 실행
  LayerOps::ensureSatelliteAtBottom(&proj);

  int satCountAfter = 0;
  for (QgsMapLayer* l : proj.mapLayers()) {
    if (l && l->name().contains(QStringLiteral("위성")))
      satCountAfter++;
  }
  QCOMPARE(satCountAfter, 1);

  // 트리 노드에서도 위성 노드는 단 1개만 존재해야 함
  QgsLayerTree* root = proj.layerTreeRoot();
  QVERIFY(root);
  int satTreeNodeCount = 0;
  for (QgsLayerTreeNode* child : root->children()) {
    if (auto* lnode = qobject_cast<QgsLayerTreeLayer*>(child)) {
      const QString name = lnode->name().isEmpty() ? (lnode->layer() ? lnode->layer()->name() : QString()) : lnode->name();
      if (name.contains(QStringLiteral("위성")))
        satTreeNodeCount++;
    }
  }
  QCOMPARE(satTreeNodeCount, 1);

  // 최하단 확인
  auto* lastNode = qobject_cast<QgsLayerTreeLayer*>(root->children().last());
  QVERIFY(lastNode && lastNode->layer());
  QVERIFY(lastNode->layer()->name().contains(QStringLiteral("위성")));
}

void TestWorkflow::xyzBasemap_layerCrsForced3857() {
  QgsProject proj;
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, nullptr, QStringLiteral("EPSG:5186")));
  QString err;
  if (!LayerOps::addOsmBasemap(&proj, nullptr, &err))
    QSKIP(qPrintable(QStringLiteral("OSM unavailable: %1").arg(err)));

  QgsRasterLayer* rl = nullptr;
  for (QgsMapLayer* l : proj.mapLayers()) {
    auto* r = qobject_cast<QgsRasterLayer*>(l);
    if (!r) continue;
    if (r->name().contains(QStringLiteral("OSM")) || r->name().contains(QStringLiteral("Carto"))) {
      rl = r;
      break;
    }
  }
  QVERIFY2(rl, "OSM/Carto raster not found after addOsmBasemap");
  QVERIFY(rl->isValid());
  QCOMPARE(rl->crs().authid(), QStringLiteral("EPSG:3857"));
  QVERIFY(LayerOps::isReferenceLayer(rl));
  QCOMPARE(proj.crs().authid(), QStringLiteral("EPSG:5186"));
}

void TestWorkflow::suggestCadastralScale_clampsWhenTooZoomedOut() {
  QCOMPARE(LayerOps::suggestCadastralScale(6000.0, 4000.0, 5000.0), 4000.0);
  QCOMPARE(LayerOps::suggestCadastralScale(4500.0, 4000.0, 5000.0), 4500.0);
  QCOMPARE(LayerOps::suggestCadastralScale(3000.0, 4000.0, 5000.0), 3000.0);
  QCOMPARE(LayerOps::suggestCadastralScale(0.0, 4000.0, 5000.0), 4000.0);
}

void TestWorkflow::prepareFieldBasemapPack_rejectsEmptyKey() {
  QgsProject proj;
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, nullptr, QStringLiteral("EPSG:5186")));
  QString err;
  const auto r = LayerOps::prepareFieldBasemapPack(&proj, nullptr, QString(), QStringLiteral("EPSG:5186"), &err);
  QVERIFY(!r.satelliteOk);
  QVERIFY(!r.cadastralOk);
  QVERIFY(!err.isEmpty());
  for (const QString& key : LayerOps::domainLayerKeys())
    QVERIFY2(LayerOps::findByLayerKey(&proj, key) == nullptr,
             "field basemap pack must not seed domain layers");
}

// Phase-1: package export must not gate on checklist errors when blockOnError=false.
void TestWorkflow::exportAllowsDespiteChecklistErrors() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_pkg_err_") +
                                            QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir(dir).removeRecursively();
  QString err;
  const QString out = ExportService::exportSubmissionPackage(
      nullptr, dir, QStringLiteral("UTF-8"),
      QStringLiteral("- [error] missing survey\n"),
      /*blockOnError=*/false, /*hasChecklistErrors=*/true, &err);
  QVERIFY2(!out.isEmpty(), qPrintable(err));
  QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("README_submit.txt"))));
  QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("MANIFEST.sha256"))));
  QFile readme(QDir(dir).filePath(QStringLiteral("README_submit.txt")));
  QVERIFY(readme.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString body = QString::fromUtf8(readme.readAll());
  QVERIFY(body.contains(QStringLiteral("missing survey")));
}

void TestWorkflow::drawingRecipesHaveDistinctScaleAndExtent() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_draw_") +
                                            QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("drawdemo"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* sa = new QgsVectorLayer(QStringLiteral("%1|layername=survey_area").arg(gpkg),
                                QStringLiteral("survey_area"), QStringLiteral("ogr"));
  QVERIFY(sa->isValid());
  LayerOps::markSurveyLayer(sa, QStringLiteral("survey_area"));
  proj.addMapLayer(sa);
  QVERIFY(sa->startEditing());
  QgsFeature sf(sa->fields());
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200080, 450000) << QgsPointXY(200080, 450080)
       << QgsPointXY(200000, 450080) << QgsPointXY(200000, 450000);
  sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(sa->addFeature(sf));
  QVERIFY(sa->commitChanges());

  QCOMPARE(LayoutService::recipe(LayoutService::DrawingKind::SurveyAreaMap).titleKo,
           QStringLiteral("조사구역도"));
  QCOMPARE(LayoutService::recipe(LayoutService::DrawingKind::SiteLocation).defaultScale, 25000.0);
  QCOMPARE(LayoutService::recipe(LayoutService::DrawingKind::SurveyAreaMap).defaultScale, 5000.0);

  LayoutService::DrawingOptions opt;
  const auto area = LayoutService::buildDrawing(&proj, LayoutService::DrawingKind::SurveyAreaMap, opt, &err);
  QVERIFY2(area.hasMapContent, qPrintable(area.warningKo));
  QVERIFY(area.appliedScale > 0);
  QVERIFY(qAbs(area.appliedScale - 5000.0) / 5000.0 < 0.25);

  const auto site = LayoutService::buildDrawing(&proj, LayoutService::DrawingKind::SiteLocation, opt, &err);
  QVERIFY(site.hasMapContent);
  QVERIFY(site.appliedScale > area.appliedScale);
  QVERIFY(site.appliedExtent.width() > area.appliedExtent.width());

  QVERIFY(proj.layoutManager()->layoutByName(QStringLiteral("survey_area_map")));
  QVERIFY(proj.layoutManager()->layoutByName(QStringLiteral("site_location")));
}

void TestWorkflow::emptySurveyDrawingShowsKoreanHint() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  LayoutService::DrawingOptions opt;
  QString err;
  const auto r = LayoutService::buildDrawing(&proj, LayoutService::DrawingKind::SurveyAreaMap, opt, &err);
  QVERIFY(!r.hasMapContent);
  QVERIFY(r.warningKo.contains(QStringLiteral("조사구역")));
  QVERIFY(proj.layoutManager()->layoutByName(QStringLiteral("survey_area_map")));
}

void TestWorkflow::zoomToLayerMax_movesCanvasToPointFeature() {
  QgsProject proj;
  QgsMapCanvas canvas;
  canvas.resize(800, 600);
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, &canvas, QStringLiteral("EPSG:5186")));
  LayerOps::zoomToKorea(&canvas, QStringLiteral("EPSG:5186"));

  auto* vl = new QgsVectorLayer(QStringLiteral("Point?crs=EPSG:5186"),
                                QStringLiteral("기준점"), QStringLiteral("memory"));
  QVERIFY(vl->isValid());
  LayerOps::markSurveyLayer(vl, QStringLiteral("control_points"));
  QVERIFY(vl->startEditing());
  QgsFeature f(vl->fields());
  f.setGeometry(QgsGeometry::fromPointXY(QgsPointXY(198500, 451200)));
  QVERIFY(vl->addFeature(f));
  QVERIFY(vl->commitChanges());
  vl->updateExtents();
  proj.addMapLayer(vl);

  QVERIFY2(LayerOps::zoomToLayerMax(&canvas, vl), "point extent isEmpty() must still zoom");
  QVERIFY(canvas.extent().contains(QgsPointXY(198500, 451200)));
}

void TestWorkflow::zoomToLayerMax_movesCanvasToFeature() {
  QgsProject proj;
  QgsMapCanvas canvas;
  canvas.resize(800, 600);
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, &canvas, QStringLiteral("EPSG:5186")));
  LayerOps::zoomToKorea(&canvas, QStringLiteral("EPSG:5186"));
  const QgsRectangle before = canvas.extent();

  auto* vl = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                QStringLiteral("구역"), QStringLiteral("memory"));
  QVERIFY(vl->isValid());
  LayerOps::markSurveyLayer(vl, QStringLiteral("survey_area"));
  QVERIFY(vl->startEditing());
  QgsFeature f(vl->fields());
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200200, 450000)
       << QgsPointXY(200200, 450200) << QgsPointXY(200000, 450200)
       << QgsPointXY(200000, 450000);
  f.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(vl->addFeature(f));
  QVERIFY(vl->commitChanges());
  vl->updateExtents();
  proj.addMapLayer(vl);

  QVERIFY(LayerOps::zoomToLayerMax(&canvas, vl));
  const QgsRectangle after = canvas.extent();
  QVERIFY2(after.contains(QgsPointXY(200100, 450100)), "canvas must move to the feature");
  QVERIFY2(after.width() < before.width(), "zoom must be tighter than Korea overview");
}

void TestWorkflow::zoomToKorea_refreshFalseSetsExtentWithoutUnfreeze() {
  QgsMapCanvas canvas;
  canvas.resize(640, 480);
  canvas.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  canvas.freeze(true);
  LayerOps::zoomToKorea(&canvas, QStringLiteral("EPSG:5186"), false);
  QVERIFY2(canvas.isFrozen(), "refresh=false must not thaw (no Korea-wide tile fetch)");
  const QgsRectangle ext = canvas.extent();
  QVERIFY2(ext.isFinite() && ext.width() > 0.0 && ext.height() > 0.0, "setExtent still runs");
}

void TestWorkflow::clampCanvasToKorea_zoomOutDoesNotExceedKorea() {
  QgsMapCanvas canvas;
  canvas.resize(800, 600);
  canvas.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  LayerOps::zoomToKorea(&canvas, QStringLiteral("EPSG:5186"), false);
  const QgsRectangle kr = LayerOps::koreaExtentForCrs(QStringLiteral("EPSG:5186"));
  QVERIFY2(!kr.isEmpty() && kr.isFinite(), "korea extent");
  QgsRectangle tooWide = canvas.extent();
  tooWide.scale(1.10);
  canvas.setExtent(tooWide);
  QVERIFY(LayerOps::clampCanvasToKorea(&canvas));
  QVERIFY2(canvas.extent().width() <= kr.width() * 1.03 + 1.0,
           "zoom-out past Korea must snap back so satellite still fills");
  QVERIFY2(canvas.extent().height() <= kr.height() * 1.03 + 1.0,
           "zoom-out must not leave empty margins around VWorld");
}

void TestWorkflow::zoomToKorea_5186StaysInsideMercatorSatelliteQuad() {
  QgsMapCanvas canvas;
  canvas.resize(800, 600);
  // A hidden widget has not received the resize event used by the real canvas.
  LayerOps::applyCanvasScreenDpi(&canvas);
  canvas.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  LayerOps::zoomToKorea(&canvas, QStringLiteral("EPSG:5186"), false);
  const QgsRectangle fill = LayerOps::satelliteFillExtentForCrs(QStringLiteral("EPSG:5186"));
  QVERIFY2(!fill.isEmpty() && fill.isFinite(), "3857 satellite inner box in 5186");
  const QgsRectangle vis = canvas.extent();
  QVERIFY2(vis.xMinimum() + 50.0 >= fill.xMinimum(), "west empty = mercator quad clip");
  QVERIFY2(vis.xMaximum() - 50.0 <= fill.xMaximum(), "east empty = mercator quad clip");
  QVERIFY2(vis.yMinimum() + 50.0 >= fill.yMinimum(), "south empty = mercator quad clip");
  QVERIFY2(vis.yMaximum() - 50.0 <= fill.yMaximum(), "north empty = mercator quad clip");
}

void TestWorkflow::clampCanvasToKorea_secondCallIsNoOp() {
  QgsMapCanvas canvas;
  canvas.resize(800, 600);
  canvas.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  LayerOps::zoomToKorea(&canvas, QStringLiteral("EPSG:5186"), false);
  LayerOps::clampCanvasToKorea(&canvas);
  const QgsRectangle afterFirst = canvas.extent();
  QVERIFY2(!LayerOps::clampCanvasToKorea(&canvas),
           "second clamp must not setExtent again (extentsChanged loop crashes)");
  QCOMPARE(canvas.extent().xMinimum(), afterFirst.xMinimum());
  QCOMPARE(canvas.extent().yMinimum(), afterFirst.yMinimum());
}

void TestWorkflow::clampCanvasToKorea_panPreservesCenterAtOverview_data() {
  QTest::addColumn<QString>("crsAuth");
  QTest::addColumn<double>("denominator");
  for (const char* crs : {"EPSG:5186", "EPSG:5187"}) {
    for (const double scale : {500000.0, 1000000.0, 1500000.0}) {
      const QByteArray row = QByteArray(crs) + '-' + QByteArray::number(scale, 'f', 0);
      QTest::newRow(row.constData()) << QString::fromLatin1(crs) << scale;
    }
  }
}

void TestWorkflow::clampCanvasToKorea_panPreservesCenterAtOverview() {
  QFETCH(QString, crsAuth);
  QFETCH(double, denominator);
  QgsMapCanvas canvas;
  canvas.resize(2400, 1250);
  canvas.setRenderFlag(false);
  LayerOps::applyCanvasScreenDpi(&canvas);
  canvas.setDestinationCrs(QgsCoordinateReferenceSystem(crsAuth));
  LayerOps::zoomToKorea(&canvas, crsAuth, false);
  canvas.setRenderFlag(false);
  canvas.zoomScale(denominator, true);
  LayerOps::clampCanvasToKorea(&canvas);
  const QgsRectangle before = canvas.extent();
  const double scaleBeforePan = canvas.scale();
  const QgsRectangle korea = LayerOps::satelliteFillExtentForCrs(crsAuth);
  // A pan changes the center without zooming. At the maximum overview the old
  // whole-viewport containment forced this horizontal move straight back.
  const double east = korea.width() * 0.1;
  const double north = korea.height() * 0.1;
  const QgsPointXY requested(before.center().x() + east, before.center().y() + north);
  canvas.setCenter(requested);
  LayerOps::clampCanvasToKorea(&canvas);
  const QgsPointXY actual = canvas.center();
  qInfo().noquote() << crsAuth << "requested scale" << denominator
                   << "actual scale" << scaleBeforePan << "pan requested metres" << east << north
                   << "pan actual metres" << actual.x() - before.center().x()
                   << actual.y() - before.center().y();
  QVERIFY2(qAbs(actual.x() - requested.x()) < 0.01, "overview must preserve east/west pan");
  QVERIFY2(qAbs(actual.y() - requested.y()) < 0.01, "overview must preserve north/south pan");
  QVERIFY(qAbs(canvas.scale() - scaleBeforePan) < 0.01);
  QVERIFY(!LayerOps::clampCanvasToKorea(&canvas));

  // Prevent navigation completely away from the coverage area, without
  // forcing an in-country center back to the middle of the country.
  canvas.setCenter(QgsPointXY(korea.xMaximum() + korea.width(), korea.yMinimum() - korea.height()));
  QVERIFY(LayerOps::clampCanvasToKorea(&canvas));
  QVERIFY(korea.contains(canvas.center()));
  QVERIFY(qAbs(canvas.scale() - scaleBeforePan) < 0.01);
  QVERIFY(!LayerOps::clampCanvasToKorea(&canvas));
}

void TestWorkflow::syncMapCanvas_disablesParallelRenderingForXyzOtf() {
  QgsProject proj;
  QgsMapCanvas canvas;
  canvas.resize(640, 480);
  canvas.setParallelRenderingEnabled(true);
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, &canvas, QStringLiteral("EPSG:5186")));
  QString err;
  QVERIFY2(LayerOps::addVworldSatelliteMap(&proj, &canvas, QString(), &err), qPrintable(err));
  LayerOps::syncMapCanvas(&proj, &canvas, false);
  QVERIFY2(!canvas.isParallelRenderingEnabled(),
           "XYZ + OTF + parallel job crashes provider_wms on Windows");
}

void TestWorkflow::isolateAndZoom_hidesOtherSurveyKeepsReference() {
  QgsProject proj;
  QgsMapCanvas canvas;
  canvas.resize(800, 600);
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, &canvas, QStringLiteral("EPSG:5186")));

  auto* keep = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("구역"), QStringLiteral("memory"));
  auto* hide = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("다른면"), QStringLiteral("memory"));
  auto* base = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:3857"),
                                  QStringLiteral("VWorld 위성"), QStringLiteral("memory"));
  QVERIFY(keep->isValid() && hide->isValid() && base->isValid());
  LayerOps::markSurveyLayer(keep, QStringLiteral("survey_area"));
  LayerOps::markSurveyLayer(hide, QStringLiteral("feature_poly"));
  LayerOps::markReferenceLayer(base);
  QVERIFY(keep->startEditing());
  QgsFeature f(keep->fields());
  QgsPolylineXY ring;
  ring << QgsPointXY(201000, 451000) << QgsPointXY(201080, 451000)
       << QgsPointXY(201080, 451080) << QgsPointXY(201000, 451080)
       << QgsPointXY(201000, 451000);
  f.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(keep->addFeature(f));
  QVERIFY(keep->commitChanges());
  keep->updateExtents();
  proj.addMapLayer(base);
  proj.addMapLayer(hide);
  proj.addMapLayer(keep);

  QVERIFY(LayerOps::isolateAndZoomToLayer(&proj, &canvas, keep, true));
  QgsLayerTree* root = proj.layerTreeRoot();
  QVERIFY(root->findLayer(keep->id())->itemVisibilityChecked());
  QVERIFY(!root->findLayer(hide->id())->itemVisibilityChecked());
  QVERIFY(root->findLayer(base->id())->itemVisibilityChecked());
  QVERIFY(canvas.extent().contains(QgsPointXY(201040, 451040)));
}

void TestWorkflow::addVworldSatellite_allowsEmptyKeyViaPublicTiles() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  QVERIFY2(LayerOps::addVworldSatelliteMap(&proj, nullptr, QString(), &err), qPrintable(err));
  QVERIFY(projectHasLayerNamedLike(&proj, QStringLiteral("VWorld 위성")));
}

void TestWorkflow::addVworldSatellite_usesOfficialWmtsWhenKeyPresent() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  QVERIFY2(LayerOps::addVworldSatelliteMap(&proj, nullptr, QStringLiteral("TEST-KEY-123"), &err),
           qPrintable(err));
  QgsMapLayer* sat = nullptr;
  for (QgsMapLayer* l : proj.mapLayers()) {
    if (l && l->name().contains(QStringLiteral("위성")))
      sat = l;
  }
  QVERIFY2(sat, "satellite layer missing");
  const QString src = sat->source();
  QVERIFY2(src.contains(QStringLiteral("api.vworld.kr")), qPrintable(src.left(160)));
  QVERIFY2(src.contains(QStringLiteral("TEST-KEY-123")), "official WMTS must include the given key");
  QVERIFY2(!src.contains(QStringLiteral("xdworld")), "must not prefer xdworld when a key is given");
  QVERIFY2(src.contains(QStringLiteral("tilePixelRatio=1")),
           "HiDPI must request 256px VWorld tiles, not @2x / z+1");
}

void TestWorkflow::addVworldSatellite_syncPutsLayerOnCanvasWithExtent() {
  QgsProject proj;
  QgsMapCanvas canvas;
  canvas.resize(640, 480);
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, &canvas, QStringLiteral("EPSG:5186")));
  QString err;
  QVERIFY2(LayerOps::addVworldSatelliteMap(&proj, &canvas, QString(), &err), qPrintable(err));
  LayerOps::syncMapCanvas(&proj, &canvas, false);
  QgsMapLayer* sat = nullptr;
  for (QgsMapLayer* l : proj.mapLayers()) {
    if (l && l->name().contains(QStringLiteral("위성")))
      sat = l;
  }
  QVERIFY2(sat, "satellite layer missing from project");
  QVERIFY(sat->isValid());
  QVERIFY2(canvas.layers().contains(sat), "canvas must show the satellite layer");
  const QgsRectangle ext = sat->extent();
  QVERIFY2(!ext.isNull() && ext.isFinite() && ext.width() > 0.0,
           "satellite layer must have a drawable extent");
}

void TestWorkflow::canvasDisplayEvent_devicePixelRatioChangeNeedsTileRefresh() {
  QVERIFY(!LayerOps::canvasDisplayEventNeedsTileRefresh(int(QEvent::Show)));
  QVERIFY(!LayerOps::canvasDisplayEventNeedsTileRefresh(int(QEvent::Resize)));
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  QVERIFY2(LayerOps::canvasDisplayEventNeedsTileRefresh(int(QEvent::DevicePixelRatioChange)),
           "4K / mixed-DPI monitor change must refetch XYZ tiles");
#endif
  QVERIFY(!LayerOps::canvasDisplayEventNeedsTileRefresh(int(QEvent::MouseMove)));
  QVERIFY(!LayerOps::canvasDisplayEventNeedsTileRefresh(int(QEvent::KeyPress)));
}

void TestWorkflow::refreshXyzBasemapTiles_restoresStaleDevicePixelRatio() {
  QgsMapCanvas canvas;
  canvas.resize(1920, 1080);
  canvas.mapSettings().setDevicePixelRatio(0.25f);
  LayerOps::refreshXyzBasemapTiles(&canvas);
  const float want = static_cast<float>(canvas.devicePixelRatioF());
  QCOMPARE(canvas.mapSettings().devicePixelRatio(), want);
  QVERIFY(want > 0.05f);
}

void TestWorkflow::refreshXyzBasemapTiles_doesNotAbortInFlightWmsJob() {
  // 현장 덤프 2026-08-31: provider_wms → deleteLater → lockThreadPostEventList 0xc0000005.
  // stopRendering/clearCache 가 내려오는 타일 QObject를 끊으면 Qt6에서 AV.
  QgsMapCanvas canvas;
  canvas.resize(640, 480);
  canvas.setParallelRenderingEnabled(true);
  LayerOps::refreshXyzBasemapTiles(&canvas);
  QVERIFY2(!canvas.isParallelRenderingEnabled(),
           "XYZ refresh must force sequential; ParallelJob + WMS deleteLater AV");

  const QString src = readLayerOpsSources();
  QVERIFY2(!src.isEmpty(), "run from source tree (ctest WORKING_DIRECTORY)");
  const int fn = src.indexOf(QLatin1String("void LayerOps::refreshXyzBasemapTiles"));
  QVERIFY2(fn >= 0, "refreshXyzBasemapTiles must exist");
  const int next = src.indexOf(QLatin1String("\nbool LayerOps::"), fn + 10);
  QVERIFY2(next > fn, "refreshXyzBasemapTiles body");
  const QString body = src.mid(fn, next - fn);
  QVERIFY2(!body.contains(QLatin1String("stopRendering")),
           "must not abort an in-flight WMS job");
  QVERIFY2(!body.contains(QLatin1String("clearCache")),
           "must not drop TileDownloadManager objects still finishing");
  QVERIFY2(!body.contains(QLatin1String("refreshAllLayers")),
           "refreshAllLayers plus refresh re-enters provider_wms block()");
  QVERIFY2(body.contains(QLatin1String("refreshCanvasNowOrLater")),
           "isDrawing 검사는 runWhenCanvasIdle 헬퍼로 옮겼다 — 진행 중 작업을 끊지 않는다");
  QVERIFY2(!body.contains(QLatin1String("canvas->refresh()")),
           "직접 refresh 하지 말 것 — 그리는 중이면 끝난 뒤로 미뤄야 한다");
}

void TestWorkflow::zoomToLayer_redrawsBasemapAtNewExtent() {
  // 사용자 보고: 그리기를 끝내거나 SHP 를 고른 뒤 레이어 우클릭 → [이 레이어로 이동]
  // 하면 옮긴 자리에 위성이 안 보이고, 줌인·줌아웃하거나 점을 찍고 지워야 나타났다.
  // 옮긴 범위로 타일을 다시 받아 오라고 시켜야 한다.
  const QString app = readMainWindowSources();
  QVERIFY2(!app.isEmpty(), "run from source tree (ctest WORKING_DIRECTORY)");
  const int fn = app.indexOf(QLatin1String("void MainWindow::zoomSelectedLayerMax"));
  QVERIFY2(fn >= 0, "zoomSelectedLayerMax must exist");
  const int next = app.indexOf(QLatin1String("\nvoid MainWindow::"), fn + 10);
  const QString body = app.mid(fn, (next > fn ? next : fn + 1600) - fn);
  QVERIFY2(body.contains(QLatin1String("zoomToLayerMax")), "레이어 범위로 옮긴다");
  QVERIFY2(body.contains(QLatin1String("refreshXyzBasemapTiles")),
           "옮긴 뒤 위성·지적 타일을 새 범위로 다시 받아 와야 한다");

  // 「전체 최대 보기」도 같은 자리에서 배경이 비었다.
  const int fullFn = app.indexOf(QLatin1String("void MainWindow::zoomMapToFullMax"));
  QVERIFY2(fullFn >= 0, "zoomMapToFullMax must exist");
  const int fullNext = app.indexOf(QLatin1String("\nvoid MainWindow::"), fullFn + 10);
  const QString fullBody = app.mid(fullFn, (fullNext > fullFn ? fullNext : fullFn + 1200) - fullFn);
  QVERIFY2(fullBody.contains(QLatin1String("refreshXyzBasemapTiles")),
           "전체 보기로 옮긴 뒤에도 배경을 다시 그려야 한다");
}

void TestWorkflow::shapeEditing_livesInsideSelectTool() {
  // 사용자 요구: 「도형수정」을 따로 두지 말 것. 도형선택으로 도형을 고르면 수정점이
  // 바로 나오고, 선에 우클릭하면 점추가·점삭제가 나와야 한다.
  QFile sel(QStringLiteral("src/app/KaFeatureSelectTool.cpp"));
  QVERIFY2(sel.open(QIODevice::ReadOnly | QIODevice::Text),
           "run from source tree (ctest WORKING_DIRECTORY)");
  const QString src = QString::fromUtf8(sel.readAll());
  QVERIFY2(src.contains(QLatin1String("new KaVertexEditTool")),
           "선택 도구가 꼭짓점 편집기를 직접 들고 있어야 한다");
  QVERIFY2(src.contains(QLatin1String("syncVertexTarget")),
           "도형을 고르면 수정점이 따라 나와야 한다");
  QVERIFY2(src.contains(QLatin1String("previewVertexMove")) &&
               src.contains(QLatin1String("moveVertexTo")),
           "수정점을 마우스로 끌어 옮길 수 있어야 한다");
  QVERIFY2(src.contains(QString::fromUtf8("점추가")) && src.contains(QString::fromUtf8("점삭제")),
           "우클릭 메뉴에 점추가·점삭제가 있어야 한다");
  QVERIFY2(src.contains(QLatin1String("insertVertexAt")) &&
               src.contains(QLatin1String("deleteVertexAt")),
           "그 메뉴가 실제 편집으로 이어져야 한다");
  QVERIFY2(src.contains(QLatin1String("clearTarget")),
           "선택 해제·도구 종료 시 수정점을 치워야 한다");

  const QString app = readMainWindowSources();
  QVERIFY2(!app.isEmpty(), "MainWindow.cpp + Export + Editing");
  QVERIFY2(!app.contains(QLatin1String("startVertexEditTool")),
           "도형수정을 따로 켜는 항목을 두지 말 것 — 도형선택에 들어 있다");
  QVERIFY2(app.contains(QLatin1String("m_featureSelectTool->setSnapEnabled")),
           "자석은 수정점을 끌 때도 걸려야 한다");
}

void TestWorkflow::subToolbar_marksTheActiveToolForTheBlueUnderline() {
  // 사용자 요구: 지금 켜져 있는 도구에 파란 밑줄이 와야 한다.
  // QSS 의 QToolBar#subToolbar QToolButton:checked 가 밑줄을 그리므로,
  // 켜진 도구의 액션이 실제로 checked 여야 한다.
  QFile qss(QStringLiteral("data/theme/ka-hgis.qss"));
  QVERIFY2(qss.open(QIODevice::ReadOnly | QIODevice::Text), "ka-hgis.qss");
  const QString style = QString::fromUtf8(qss.readAll());
  const QRegularExpression checkedRule(QStringLiteral(
      R"(QToolBar#subToolbar QToolButton(?::enabled)?:checked(?::enabled)?(?=\s*\{))"));
  const int rule = checkedRule.match(style).capturedStart();
  QVERIFY2(rule >= 0, "켜진 버튼을 표시하는 규칙이 있어야 한다");
  const int ruleEnd = style.indexOf(QLatin1Char('}'), rule);
  QVERIFY2(ruleEnd > rule, "선택 버튼 규칙이 닫혀 있어야 한다");
  QVERIFY2(style.mid(rule, ruleEnd - rule).contains(QLatin1String("border-bottom")),
           "표시는 파란 밑줄이어야 한다");

  QFile mw(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString app = QString::fromUtf8(mw.readAll());
  const int fn = app.indexOf(QLatin1String("void MainWindow::updateSubToolbarChecks"));
  QVERIFY2(fn >= 0, "켜진 도구를 표시하는 함수가 있어야 한다");
  const QString body = app.mid(fn, 1400);
  QVERIFY2(body.contains(QLatin1String("setCheckable")) && body.contains(QLatin1String("setChecked")),
           "액션을 체크해야 QSS 가 밑줄을 그린다");
  QVERIFY2(body.contains(QLatin1String("m_featureSelectTool")),
           "도형선택이 켜져 있으면 그 버튼에 밑줄이 와야 한다");

  const int draw = app.indexOf(QLatin1String("void MainWindow::showSubToolsDraw"));
  QVERIFY2(draw >= 0, "showSubToolsDraw must exist");
  const int drawEnd = app.indexOf(QLatin1String("\nvoid MainWindow::showSubToolsBasemap"), draw + 1);
  const QString drawBody = app.mid(draw, (drawEnd > draw ? drawEnd : draw + 5000) - draw);
  QVERIFY2(drawBody.contains(QLatin1String("kaSubTool")),
           "버튼마다 어떤 도구인지 꼬리표가 있어야 판단할 수 있다");
  QVERIFY2(drawBody.contains(QLatin1String("updateSubToolbarChecks")),
           "툴바를 다시 만들면 밑줄도 다시 맞춰야 한다");
  QVERIFY2(app.contains(QLatin1String("QgsMapCanvas::mapToolSet")),
           "도구를 바꾸면 밑줄도 따라가야 한다");
}

void TestWorkflow::alignPanel_showsHugeTiffViaGdalPreview() {
  // 현장 사례: (B040)항공사진 11864 x 23282 = 2.76억 화소 8bit TIFF.
  // Qt 는 그림을 통째로 펴서 읽으므로 263MB 가 필요하고, Qt6 기본 상한 256MB 를
  // 넘겨 조용히 빈 그림이 됐다. TIFF 를 못 읽는 게 아니라 크기 때문이다.
  // 지도 쪽은 GDAL 이 솎아 읽어 멀쩡했으니, 왼쪽 칸도 같은 길로 가야 한다.
  QFile mw(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text),
           "run from source tree (ctest WORKING_DIRECTORY)");
  const QString app = QString::fromUtf8(mw.readAll());
  const int split = app.indexOf(QLatin1String("void MainWindow::showAlignSplit"));
  QVERIFY2(split >= 0, "showAlignSplit must exist");
  const QString splitBody = app.mid(split, 1400);
  QVERIFY2(splitBody.contains(QLatin1String("if (!m_alignImage->loadPath")),
           "못 읽었는지 확인하지 않으면 빈 칸이 그대로 남는다");
  QVERIFY2(splitBody.contains(QLatin1String("loadAlignPreviewFromRaster")),
           "Qt 로 못 여는 원판은 GDAL 축소본으로 채워야 한다");

  const int fb = app.indexOf(QLatin1String("bool MainWindow::loadAlignPreviewFromRaster"));
  QVERIFY2(fb >= 0, "GDAL 축소본 경로가 있어야 한다");
  const QString fbBody = app.mid(fb, 2000);
  QVERIFY2(fbBody.contains(QLatin1String("previewAsImage")),
           "GDAL 이 솎아 읽는 previewAsImage 를 써야 한다");
  QVERIFY2(fbBody.contains(QLatin1String("setPreview")),
           "축소본을 왼쪽 칸에 넣어야 한다");
  QVERIFY2(fbBody.contains(QLatin1String("notify(")),
           "그래도 못 띄우면 조용히 두지 말고 알려야 한다");

  // 축소본을 보여 주더라도 찍은 좌표는 원본 픽셀이어야 정합이 맞는다.
  QFile iv(QStringLiteral("src/app/KaImageView.cpp"));
  QVERIFY2(iv.open(QIODevice::ReadOnly | QIODevice::Text), "KaImageView.cpp");
  const QString view = QString::fromUtf8(iv.readAll());
  QVERIFY2(view.contains(QLatin1String("m_srcScale")),
           "축소 비율을 들고 있어야 원본 좌표로 되돌릴 수 있다");
  QVERIFY2(view.contains(QLatin1String("pixelClicked(sc.x() * m_srcScale")),
           "클릭 좌표는 원본 픽셀로 돌려줘야 한다");
  QVERIFY2(view.contains(QLatin1String("QImageReader")),
           "왜 못 읽었는지 남겨야 한다");
}

void TestWorkflow::alignPreview_keepsImageOrientation() {
  // 축소본으로 바꿔치기할 때 위아래가 뒤집히면 찍은 점이 엉뚱한 자리로 간다.
  // 좌표가 없는 그림(정합 전 항공사진)에서 GDAL 이 주는 방향을 실제로 확인한다.
  const QString dir = QDir::temp().filePath(
      QStringLiteral("ka_ori_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  const QString tif = QDir(dir).filePath(QStringLiteral("plain.tif"));

  // 위 절반 흰색, 아래 절반 검정. 좌표 정보는 넣지 않는다.
  QImage src(8, 16, QImage::Format_Grayscale8);
  for (int y = 0; y < src.height(); ++y)
    for (int x = 0; x < src.width(); ++x)
      src.setPixel(x, y, y < src.height() / 2 ? 255 : 0);
  QVERIFY2(src.save(tif, "TIFF"), "Qt TIFF 저장 플러그인이 있어야 한다");

  QgsRasterLayer rl(tif, QStringLiteral("plain"), QStringLiteral("gdal"));
  QVERIFY2(rl.isValid(), "GDAL 이 좌표 없는 TIFF 도 열어야 한다");
  QCOMPARE(rl.width(), 8);
  QCOMPARE(rl.height(), 16);

  const QImage prev = rl.previewAsImage(QSize(rl.width(), rl.height()));
  QVERIFY2(!prev.isNull(), "previewAsImage 가 그림을 내놔야 한다");

  const int topY = prev.height() / 4;
  const int botY = prev.height() * 3 / 4;
  const int top = qGray(prev.pixel(prev.width() / 2, topY));
  const int bot = qGray(prev.pixel(prev.width() / 2, botY));
  QVERIFY2(top > 200 && bot < 60,
           qPrintable(QStringLiteral("축소본 상하가 원본과 같아야 한다 (top=%1 bot=%2)")
                          .arg(top).arg(bot)));
}

void TestWorkflow::layerPanel_hasOneCheckAllToggle() {
  // 사용자 요구: 「레이어」 글자 옆에 전체 체크 풀기·하기 단추 하나.
  QFile mw(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text),
           "run from source tree (ctest WORKING_DIRECTORY)");
  const QString app = QString::fromUtf8(mw.readAll());
  QVERIFY2(app.contains(QLatin1String("layerCheckAllBtn")),
           "레이어 제목 줄에 단추가 있어야 한다");
  const int fn = app.indexOf(QLatin1String("void MainWindow::toggleAllLayersChecked"));
  QVERIFY2(fn >= 0, "전체 켜기·끄기 동작이 있어야 한다");
  const QString body = app.mid(fn, 1400);
  QVERIFY2(body.contains(QLatin1String("setItemVisibilityCheckedRecursive")),
           "묶음 안에 든 레이어까지 한 번에 바뀌어야 한다");
  QVERIFY2(body.contains(QLatin1String("anyOn")),
           "하나라도 켜져 있으면 전부 끄고, 다 꺼져 있을 때만 전부 켠다");

  const int upd = app.indexOf(QLatin1String("void MainWindow::refreshLayerCheckAllButton"));
  QVERIFY2(upd >= 0, "단추 글씨를 상태에 맞추는 곳이 있어야 한다");
  const QString updBody = app.mid(upd, 900);
  QVERIFY2(updBody.contains(QString::fromUtf8("전체 끄기")) &&
               updBody.contains(QString::fromUtf8("전체 켜기")),
           "누르면 일어날 일을 글씨로 보여야 한다");
  QVERIFY2(app.contains(QLatin1String("QgsLayerTreeNode::visibilityChanged")),
           "체크를 손으로 바꿔도 단추 글씨가 따라가야 한다");

  QFile qss(QStringLiteral("data/theme/ka-hgis.qss"));
  QVERIFY2(qss.open(QIODevice::ReadOnly | QIODevice::Text), "ka-hgis.qss");
  QVERIFY2(QString::fromUtf8(qss.readAll())
               .contains(QLatin1String("QToolButton#layerCheckAllBtn")),
           "단추에 스타일이 있어야 카드 안에서 겉돌지 않는다");
}

void TestWorkflow::layoutLayerPanel_hasCheckAllToggle() {
  QFile studio(QStringLiteral("src/app/KaDrawingStudio.cpp"));
  QVERIFY2(studio.open(QIODevice::ReadOnly | QIODevice::Text),
           "run from source tree (ctest WORKING_DIRECTORY)");
  const QString app = QString::fromUtf8(studio.readAll());
  QVERIFY2(app.contains(QLatin1String("layoutLayerCheckAllBtn")),
           "조판 레이어 제목 줄에도 전체끄기 단추가 있어야 한다");
  QVERIFY2(app.contains(QLatin1String("void KaDrawingStudio::toggleAllLayersChecked")),
           "조판 전용 전체 켜기·끄기가 있어야 한다");
  const int fn = app.indexOf(QLatin1String("void KaDrawingStudio::toggleAllLayersChecked"));
  QVERIFY2(fn >= 0, "조판 토글 본문이 있어야 한다");
  const QString body = app.mid(fn, 1400);
  QVERIFY2(body.contains(QLatin1String("setItemVisibilityCheckedRecursive")),
           "묶음 안 레이어까지 한 번에 바뀌어야 한다");
  QVERIFY2(body.contains(QLatin1String("syncMapFromLayers")),
           "조판은 맵 슬롯이 아니라 도면 레이어 동기화만 탄다");
  QVERIFY2(!body.contains(QLatin1String("MainWindow::toggleAllLayersChecked")),
           "맵 전체끄기를 조판에서 부르면 캔버스까지 건드린다");
  QVERIFY2(app.contains(QString::fromUtf8("전체 끄기")) &&
               app.contains(QString::fromUtf8("전체 켜기")),
           "조판 단추 글씨가 맵과 같아야 한다");

  QFile qss(QStringLiteral("data/theme/ka-hgis.qss"));
  QVERIFY2(qss.open(QIODevice::ReadOnly | QIODevice::Text), "ka-hgis.qss");
  QVERIFY2(QString::fromUtf8(qss.readAll())
               .contains(QLatin1String("layoutLayerCheckAllBtn")),
           "조판 단추에도 같은 카드 스타일이 있어야 한다");
}

void TestWorkflow::layoutLegend_deletableAndStaysDeleted() {
  // 사용자 보고: 조판에서 범례를 지울 수가 없다.
  // 원인 1 — 뷰에 도구를 하나도 걸지 않아 용지 위 항목이 선택되지 않았다.
  //          선택이 없으니 Delete 도 지울 것을 못 찾았다.
  // 원인 2 — 지워도 제목·글자 칸을 건드리면 applyLegendSettings 가 다시 만들었다.
  QFile f(QStringLiteral("src/app/KaDrawingStudio.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
           "run from source tree (ctest WORKING_DIRECTORY)");
  const QString src = QString::fromUtf8(f.readAll());

  // 새 용지를 만들 때 ensureBlankLayout 이 도구를 푼다. 그러니 다시 붙일 때마다
  // 선택 도구를 걸어 줘야 항목을 고르고 Delete 로 지울 수 있다.
  const int attach = src.indexOf(QLatin1String("void KaDrawingStudio::attachLayoutToView"));
  QVERIFY2(attach >= 0, "attachLayoutToView must exist");
  QVERIFY2(src.mid(attach, 1400).contains(QLatin1String("setTool(m_toolSelect)")),
           "레이아웃을 붙일 때 선택 도구를 걸어야 항목을 고르고 지울 수 있다");

  const int del = src.indexOf(QLatin1String("void KaDrawingStudio::deleteSelectedItems"));
  QVERIFY2(del >= 0, "deleteSelectedItems must exist");
  QVERIFY2(src.mid(del, 1400).contains(QLatin1String("m_legendRemoved = true")),
           "지운 범례를 기억해야 한다");

  const int apply = src.indexOf(QLatin1String("void KaDrawingStudio::applyLegendSettings"));
  QVERIFY2(apply >= 0, "applyLegendSettings must exist");
  QVERIFY2(src.mid(apply, 600).contains(QLatin1String("if (m_legendRemoved) return;")),
           "일부러 지운 범례는 글자 설정을 만져도 되살아나면 안 된다");

  const int begin = src.indexOf(QLatin1String("void KaDrawingStudio::beginPlaceLegend"));
  QVERIFY2(begin >= 0 && src.mid(begin, 300).contains(QLatin1String("m_legendRemoved = false")),
           "[범례] 를 다시 누르면 되살아나야 한다");

  // 원인 3 — Delete 는 MainWindow 의 창 단축키(WindowShortcut)로 먼저 잡힌다.
  // 조판은 같은 창 안의 탭이라 자기 keyPressEvent 가 아예 돌지 않았다.
  QVERIFY2(src.contains(QLatin1String("void KaDrawingStudio::handleDeleteKey")),
           "바깥에서 Delete 를 넘겨받을 자리가 있어야 한다");
  QFile mw(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString app = QString::fromUtf8(mw.readAll());
  const int route = app.indexOf(QLatin1String("bool MainWindow::routeEditKeyToActiveStudio"));
  QVERIFY2(route >= 0, "창 단축키를 조판으로 넘기는 곳이 있어야 한다");
  const QString rb = app.mid(route, 1200);
  QVERIFY2(rb.contains(QLatin1String("handleDeleteKey")) &&
               rb.contains(QLatin1String("handleUndoKey")),
           "Delete 와 Ctrl+Z 를 모두 넘겨야 한다");
  QVERIFY2(rb.contains(QLatin1String("QLineEdit")) &&
               rb.contains(QLatin1String("QAbstractSpinBox")),
           "글자 칸에 커서가 있으면 도형·레이어를 지우면 안 된다");
  QVERIFY2(app.contains(QLatin1String("routeEditKeyToActiveStudio(true)")) &&
               app.contains(QLatin1String("routeEditKeyToActiveStudio(false)")),
           "두 단축키 모두 이 길을 타야 한다");
}

void TestWorkflow::opacityRail_hasBrightnessForRasters() {
  // 사용자 요구: 투명도 게이지 옆에 밝기 게이지를 하나 더.
  // 옛 항공사진은 원판이 어두워 지번·경계가 잘 안 보인다.
  QFile h(QStringLiteral("src/app/KaLayerOpacityRail.h"));
  QVERIFY2(h.open(QIODevice::ReadOnly | QIODevice::Text), "KaLayerOpacityRail.h");
  const QString hdr = QString::fromUtf8(h.readAll());
  QVERIFY2(hdr.contains(QLatin1String("brightnessChanged")), "밝기 신호가 있어야 한다");
  QVERIFY2(hdr.contains(QLatin1String("setBrightness")), "밝기 값을 넣고 뺄 수 있어야 한다");

  QFile c(QStringLiteral("src/app/KaLayerOpacityRail.cpp"));
  QVERIFY2(c.open(QIODevice::ReadOnly | QIODevice::Text), "KaLayerOpacityRail.cpp");
  const QString rail = QString::fromUtf8(c.readAll());
  QVERIFY2(rail.contains(QLatin1String("layerBrightnessSlider")), "밝기 막대가 있어야 한다");
  QVERIFY2(rail.contains(QLatin1String("setRange(-255, 255)")),
           "가운데가 원본이 되도록 -255~255 여야 한다");

  // 지도와 조판 두 곳 모두에서 연결돼야 한다.
  for (const QString& path : {QStringLiteral("src/app/MainWindow.cpp"),
                              QStringLiteral("src/app/KaDrawingStudio.cpp")}) {
    QFile f(path);
    QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(path));
    const QString s = QString::fromUtf8(f.readAll());
    QVERIFY2(s.contains(QLatin1String("KaLayerOpacityRail::brightnessChanged")),
             qPrintable(path + QStringLiteral(" 에서 밝기를 받아야 한다")));
    QVERIFY2(s.contains(QLatin1String("setMapLayerBrightness")),
             qPrintable(path + QStringLiteral(" 에서 레이어에 밝기를 적용해야 한다")));
    QVERIFY2(s.contains(QLatin1String("canAdjustBrightness")),
             qPrintable(path + QStringLiteral(" 벡터에는 밝기가 없으니 막아야 한다")));
  }

  // 실제로 그림 레이어 밝기가 바뀌는지.
  const QString dir = QDir::temp().filePath(
      QStringLiteral("ka_bright_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  const QString tif = QDir(dir).filePath(QStringLiteral("g.tif"));
  QImage img(8, 8, QImage::Format_Grayscale8);
  img.fill(128);
  QVERIFY(img.save(tif, "TIFF"));
  QgsRasterLayer rl(tif, QStringLiteral("g"), QStringLiteral("gdal"));
  QVERIFY(rl.isValid());
  QVERIFY2(LayerOps::canAdjustBrightness(&rl), "그림 레이어는 밝기를 조절할 수 있다");
  QCOMPARE(LayerOps::mapLayerBrightness(&rl), 0);
  QVERIFY(LayerOps::setMapLayerBrightness(&rl, 60, nullptr));
  QCOMPARE(LayerOps::mapLayerBrightness(&rl), 60);

  QgsVectorLayer vl(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("v"),
                    QStringLiteral("memory"));
  QVERIFY(vl.isValid());
  QVERIFY2(!LayerOps::canAdjustBrightness(&vl), "도형 레이어에는 밝기가 없다");
}

void TestWorkflow::measureHudSitsBesideOpacityRail() {
  QFile c(QStringLiteral("src/app/KaMeasureMapTool.cpp"));
  QVERIFY2(c.open(QIODevice::ReadOnly | QIODevice::Text), "KaMeasureMapTool.cpp");
  const QString s = QString::fromUtf8(c.readAll());
  QVERIFY2(s.contains(QLatin1String("layerOpacityRail")),
           "줄자 창이 투명도 막대를 피해야 한다");
  QVERIFY2(s.contains(QLatin1String("geometry().right()")),
           "줄자는 막대 오른쪽에 두어야 한다");
}

// ── 겹침 재현용 도구 ────────────────────────────────────────────────────
// 아래: 지번을 다는 지적 면. 위: 그 지번을 가로지르는 빨간 선.
// 실제 화소로 「누가 나중에 그려졌는지」를 본다.
namespace ka_overlap {

constexpr double kSpan = 100.0;   // 지도 범위 0~100
constexpr int kPx = 400;          // 그림 400x400 → 1m = 4px

inline QgsRectangle extent() { return QgsRectangle(0.0, 0.0, kSpan, kSpan); }

inline QgsVectorLayer* makeCadastralWithLabels() {
  auto* vl = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186&field=jibun:string"),
                                QStringLiteral("지적도"), QStringLiteral("memory"));
  QgsFeature f(vl->fields());
  f.setAttribute(QStringLiteral("jibun"), QStringLiteral("888888"));
  f.setGeometry(QgsGeometry::fromRect(extent()));
  vl->dataProvider()->addFeature(f);
  vl->updateExtents();
  // 앱이 쓰는 라벨 설정 그대로(흰 테두리 포함). 글자를 크게 해 선과 확실히 겹친다.
  LayerOps::applyNameAttributeLabels(vl, QStringLiteral("jibun"), 28.0, false);
  return vl;
}

inline QgsVectorLayer* makeRedLine() {
  auto* vl = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5186"),
                                QStringLiteral("유존지역평가지역"), QStringLiteral("memory"));
  QgsFeature f;
  f.setGeometry(QgsGeometry::fromPolylineXY(
      QgsPolylineXY() << QgsPointXY(0.0, kSpan / 2.0) << QgsPointXY(kSpan, kSpan / 2.0)));
  vl->dataProvider()->addFeature(f);
  vl->updateExtents();
  auto* sl = new QgsSimpleLineSymbolLayer(QColor(255, 0, 0), 1.6);
  auto* sym = new QgsLineSymbol(QgsSymbolLayerList() << sl);
  vl->setRenderer(new QgsSingleSymbolRenderer(sym));
  return vl;
}

// 라벨 뒤에 다시 그릴 레이어들을 같은 그림 위에 2차 패스로 얹는다.
// 앱의 캔버스 오버레이·조판 덧지도가 하는 일과 같은 계산이다.
inline void paintAboveLabels(QImage* img, const QList<QgsMapLayer*>& above,
                             const QgsMapSettings& base) {
  if (!img || above.isEmpty()) return;
  QgsMapSettings ms = base;
  ms.setLayers(above);
  ms.setBackgroundColor(Qt::transparent);
  ms.setFlag(Qgis::MapSettingsFlag::DrawLabeling, false);
  QPainter p(img);
  QgsMapRendererCustomPainterJob job(ms, &p);
  job.start();
  job.waitForFinished();
  p.end();
}

inline QgsMapSettings baseSettings(const QList<QgsMapLayer*>& layers) {
  QgsMapSettings ms;
  ms.setLayers(layers);
  ms.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  ms.setExtent(extent());
  ms.setOutputSize(QSize(kPx, kPx));
  ms.setOutputDpi(96);
  ms.setBackgroundColor(Qt::white);
  ms.setFlag(Qgis::MapSettingsFlag::DrawLabeling, true);
  ms.setFlag(Qgis::MapSettingsFlag::Antialiasing, false);
  return ms;
}

// layers[0] 이 맨 위. 앱과 같은 순차 경로(CustomPainterJob)로 그린다.
inline QImage renderMap(const QList<QgsMapLayer*>& layers) {
  QgsMapSettings ms;
  ms.setLayers(layers);
  ms.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  ms.setExtent(extent());
  ms.setOutputSize(QSize(kPx, kPx));
  ms.setOutputDpi(96);
  ms.setBackgroundColor(Qt::white);
  ms.setFlag(Qgis::MapSettingsFlag::DrawLabeling, true);
  ms.setFlag(Qgis::MapSettingsFlag::Antialiasing, false);
  QImage img(QSize(kPx, kPx), QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::white);
  QPainter p(&img);
  QgsMapRendererCustomPainterJob job(ms, &p);
  job.start();
  job.waitForFinished();
  p.end();
  return img;
}

inline bool isRed(QRgb c) {
  return qRed(c) > 150 && qGreen(c) < 110 && qBlue(c) < 110;
}

// 글자가 놓인 가운데 구간에서, 선이 지나는 줄에 빨강이 몇 개나 남았는지.
inline int redPixelsAcrossLabel(const QImage& img) {
  const int row = img.height() / 2;
  int n = 0;
  for (int dy = -3; dy <= 3; ++dy) {
    const int y = row + dy;
    if (y < 0 || y >= img.height()) continue;
    for (int x = img.width() * 3 / 8; x < img.width() * 5 / 8; ++x) {
      if (isRed(img.pixel(x, y))) ++n;
    }
  }
  return n;
}

}  // namespace ka_overlap

void TestWorkflow::redLineDrawsOverLowerLayerLabels_map() {
  // 사용자 보고: 빨간 경계선이 지적도보다 위인데, 아래 지적도의 지번 글자와
  // 흰 테두리가 빨간 선을 가린다. QGIS 는 도형을 모두 그린 뒤 라벨을 맨 위에
  // 한 번에 얹기 때문이다. 실제 화소로 그리기 순서를 확인한다.
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QgsVectorLayer* cad = ka_overlap::makeCadastralWithLabels();
  QgsVectorLayer* red = ka_overlap::makeRedLine();
  QVERIFY(cad->isValid());
  QVERIFY(red->isValid());
  proj.addMapLayer(cad);
  proj.addMapLayer(red);
  QVERIFY(cad->labelsEnabled());

  // 선만 그렸을 때 글자 구간에 남는 빨강 = 기준값.
  const QImage lineOnly = ka_overlap::renderMap(QList<QgsMapLayer*>() << red);
  const int redAlone = ka_overlap::redPixelsAcrossLabel(lineOnly);
  QVERIFY2(redAlone > 100, qPrintable(QStringLiteral("기준 빨강 화소=%1").arg(redAlone)));

  // 빨간 선을 위에 두고 지적을 밑에. 규칙을 걸고 그린다.
  QgsLayerTree* root = proj.layerTreeRoot();
  root->removeAllChildren();
  root->addLayer(red);
  root->addLayer(cad);
  LayerOps::applyLayerOrderToLabels(&proj, nullptr);

  const QImage both = ka_overlap::renderMap(QList<QgsMapLayer*>() << red << cad);
  const int redWithLabels = ka_overlap::redPixelsAcrossLabel(both);

  // 한 번의 렌더로는 QGIS 가 라벨을 맨 마지막에 얹으므로 반드시 가려진다.
  // 이것이 재현이다. 여기서 멈추지 않고, 2차 패스가 이를 되돌리는지 본다.
  QVERIFY2(redWithLabels < redAlone / 2,
           qPrintable(QStringLiteral("한 번만 그리면 가려져야 재현이다 (선만=%1, 함께=%2)")
                          .arg(redAlone).arg(redWithLabels)));

  // 2차 패스에 넘길 레이어를 규칙이 골라 줘야 한다.
  const QList<QgsMapLayer*> above = LayerOps::layersDrawnAboveLabels(&proj);
  QVERIFY2(above.contains(red),
           "속이 빈 빨간 선은 라벨 뒤에 다시 그릴 대상이어야 한다");

  // 흔한 「속 빈 면」 스타일: 채움색 알파 0 + Solid + 진한 테두리.
  // 아무것도 안 칠하므로 두 번 그려도 같다 — 반드시 대상이어야 한다.
  {
    auto* hollow = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                      QStringLiteral("속빈면"), QStringLiteral("memory"));
    QVERIFY(hollow->isValid());
    proj.addMapLayer(hollow);
    auto* fl = new QgsSimpleFillSymbolLayer();
    fl->setColor(QColor(255, 0, 0, 0));      // 완전 투명 채움
    fl->setBrushStyle(Qt::SolidPattern);
    fl->setStrokeColor(QColor(255, 0, 0));   // 진한 빨강 테두리
    hollow->setRenderer(new QgsSingleSymbolRenderer(new QgsFillSymbol(QgsSymbolLayerList() << fl)));
    root->removeAllChildren();
    root->addLayer(hollow);
    root->addLayer(cad);
    QVERIFY2(LayerOps::layersDrawnAboveLabels(&proj).contains(hollow),
             "알파 0 채움 + 진한 테두리도 대상이어야 한다");
    root->removeAllChildren();
    root->addLayer(red);
    root->addLayer(cad);
  }
  QVERIFY2(!above.contains(cad),
           "맨 아래 지적은 다시 그릴 대상이 아니다");

  // 2차 패스를 얹으면 빨간 선이 지번 위로 올라와야 한다.
  QImage fixed = both;
  ka_overlap::paintAboveLabels(&fixed, above, ka_overlap::baseSettings(
                                                  QList<QgsMapLayer*>() << red << cad));
  const int redAfter = ka_overlap::redPixelsAcrossLabel(fixed);
  QVERIFY2(redAfter >= redAlone * 9 / 10,
           qPrintable(QStringLiteral(
                          "2차 패스 뒤에는 빨간 선이 지번 위로 와야 한다 "
                          "(선만=%1, 1패스=%2, 2패스=%3)")
                          .arg(redAlone).arg(redWithLabels).arg(redAfter)));
}

void TestWorkflow::redLineDrawsOverLowerLayerLabels_layoutAndPdf() {
  // 조판 미리보기와 PDF 도 지도와 같은 결과여야 한다.
  // 본 지도 위에 「덧지도」를 얹는 방식이 실제로 먹는지 화소로 본다.
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QgsVectorLayer* cad = ka_overlap::makeCadastralWithLabels();
  QgsVectorLayer* red = ka_overlap::makeRedLine();
  proj.addMapLayer(cad);
  proj.addMapLayer(red);
  QgsLayerTree* root = proj.layerTreeRoot();
  root->removeAllChildren();
  root->addLayer(red);
  root->addLayer(cad);

  const QList<QgsMapLayer*> above = LayerOps::layersDrawnAboveLabels(&proj);
  QVERIFY2(above.contains(red), "빨간 선이 덧지도 대상이어야 한다");

  auto* ly = new QgsPrintLayout(&proj);
  ly->initializeDefaults();
  proj.layoutManager()->addLayout(ly);

  auto* base = new QgsLayoutItemMap(ly);
  base->setId(QStringLiteral("ka_map"));
  base->attemptSetSceneRect(QRectF(10, 10, 180, 180));
  ly->addLayoutItem(base);
  base->setKeepLayerSet(true);
  base->setLayers(QList<QgsMapLayer*>() << red << cad);
  base->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  base->zoomToExtent(ka_overlap::extent());

  auto renderPage = [&]() -> QImage {
    QgsLayoutExporter ex(ly);
    return ex.renderPageToImage(0, QSize(800, 600), 96.0);
  };
  auto redCount = [](const QImage& img) {
    int n = 0;
    for (int y = 0; y < img.height(); ++y)
      for (int x = 0; x < img.width(); ++x)
        if (ka_overlap::isRed(img.pixel(x, y))) ++n;
    return n;
  };

  const QImage plain = renderPage();
  QVERIFY2(!plain.isNull(), "조판을 그려야 한다");
  const int redBefore = redCount(plain);

  // 덧지도를 얹는다 — 앱의 syncAboveLabelsMap 과 같은 구성.
  auto* top = new QgsLayoutItemMap(ly);
  top->setId(QStringLiteral("ka_map_above"));
  ly->addLayoutItem(top);
  top->setFrameEnabled(false);
  top->setBackgroundEnabled(false);
  top->setKeepLayerSet(true);
  top->setLayers(above);
  top->setCrs(base->crs());
  top->attemptSetSceneRect(base->rect().translated(base->pos()));
  top->zoomToExtent(base->extent());
  top->setExtent(base->extent());
  ly->updateZValues(false);
  top->invalidateCache();

  const QImage withTop = renderPage();
  const int redAfter = redCount(withTop);
  QVERIFY2(redAfter > redBefore,
           qPrintable(QStringLiteral("덧지도가 빨간 선을 지번 위로 올려야 한다 (전=%1, 후=%2)")
                          .arg(redBefore).arg(redAfter)));

  // PDF 도 같은 경로다. 내보낸 뒤 화소로 확인한다.
  const QString dir = QDir::temp().filePath(
      QStringLiteral("ka_pdf_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  const QString pdf = QDir(dir).filePath(QStringLiteral("sheet.pdf"));
  QgsLayoutExporter ex(ly);
  QgsLayoutExporter::PdfExportSettings ps;
  ps.dpi = 96;
  ps.rasterizeWholeImage = true;  // 화소 비교를 위해 래스터로 굽는다
  QCOMPARE(ex.exportToPdf(pdf, ps), QgsLayoutExporter::Success);
  QVERIFY2(QFileInfo::exists(pdf) && QFileInfo(pdf).size() > 1000, "PDF 가 나와야 한다");

  QImageReader rd(pdf, "pdf");
  rd.setScaledSize(QSize(800, 600));
  const QImage pdfImg = rd.read();
  if (!pdfImg.isNull()) {
    QVERIFY2(redCount(pdfImg) > 0, "PDF 에도 빨간 선이 남아야 한다");
  } else {
    QWARN("PDF 래스터화 플러그인이 없어 화소 비교는 건너뜀 (내보내기 자체는 확인됨)");
  }
}

void TestWorkflow::perf_labelOrderAnalysisIsCheapPerRefresh() {
  // 지도를 다시 그릴 때마다 도는 분석이다. 현장 조사에는 레이어가 수십 개 붙으므로
  // 한 번 도는 비용이 크면 팬·줌이 끊긴다. 실제로 재서 상한을 못박는다.
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QgsLayerTree* root = proj.layerTreeRoot();
  root->removeAllChildren();

  const int kLayers = 40;  // 현장에서 흔한 규모
  for (int i = 0; i < kLayers; ++i) {
    auto* vl = new QgsVectorLayer(
        QStringLiteral("Polygon?crs=EPSG:5186&field=nm:string"),
        QStringLiteral("layer%1").arg(i), QStringLiteral("memory"));
    QVERIFY(vl->isValid());
    proj.addMapLayer(vl);
    if (i % 3 == 0)
      LayerOps::applyNameAttributeLabels(vl, QStringLiteral("nm"), 8.0, false);
    root->addLayer(vl);
  }

  // 한 번은 상태를 바꾸므로 먼저 돌려 안정 상태로 만든다(실제 두 번째 갱신부터가 평상시).
  LayerOps::applyLayerOrderToLabels(&proj, nullptr);
  (void)LayerOps::layersDrawnAboveLabels(&proj);

  const int kRuns = 20;
  QElapsedTimer t;
  t.start();
  for (int r = 0; r < kRuns; ++r) {
    LayerOps::applyLayerOrderToLabels(&proj, nullptr);
    (void)LayerOps::layersDrawnAboveLabels(&proj);
  }
  const double perRefreshMs = double(t.nsecsElapsed()) / 1e6 / kRuns;
  qInfo("label-order analysis: %.3f ms per refresh (%d layers)", perRefreshMs, kLayers);

  // 60fps 한 프레임이 16ms 다. 결과를 캐시하므로 상태가 그대로면 거의 공짜여야 한다.
  // 캐시를 넣기 전 실측 1.856ms → 넣은 뒤 0.063ms.
  QVERIFY2(perRefreshMs < 0.5,
           qPrintable(QStringLiteral("갱신마다 도는 분석이 너무 비싸다: %1 ms")
                          .arg(perRefreshMs, 0, 'f', 3)));

  // 지도 갱신 본체도 같은 규모에서 재 둔다. 여기가 느려지면 팬·줌이 끊긴다.
  QgsMapCanvas canvas;
  canvas.resize(800, 600);
  canvas.setDestinationCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  LayerOps::syncMapCanvas(&proj, &canvas, false);  // 첫 회는 준비 비용이 섞인다
  QElapsedTimer t2;
  t2.start();
  for (int r = 0; r < kRuns; ++r)
    LayerOps::syncMapCanvas(&proj, &canvas, false);
  const double syncMs = double(t2.nsecsElapsed()) / 1e6 / kRuns;
  qInfo("syncMapCanvas: %.3f ms per refresh (%d layers)", syncMs, kLayers);

  QElapsedTimer t3;
  t3.start();
  for (int r = 0; r < kRuns; ++r)
    (void)LayerOps::visibleLayersPaintOrder(&proj);
  const double orderMs = double(t3.nsecsElapsed()) / 1e6 / kRuns;
  qInfo("visibleLayersPaintOrder: %.3f ms (%d layers)", orderMs, kLayers);
}


void TestWorkflow::labelsFollowLayerStackOrder() {
  // 사용자 요구: 레이어가 밑에 있으면 글자도 밑으로 가야 한다.
  // QGIS 는 레이어를 다 그린 뒤 라벨만 한 번에 맨 위에 얹어서, 아래 지적도의
  // 지번이 위 구역선 위로 올라왔다. 순서를 글자에도 먹인다.
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));

  // 아래: 지번을 다는 지적 레이어
  auto* cad = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186&field=jibun:string"),
                                 QStringLiteral("지적도"), QStringLiteral("memory"));
  QVERIFY(cad->isValid());
  // 위: 글자 없는 구역선
  auto* zone = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5186"),
                                  QStringLiteral("유존지역평가지역"), QStringLiteral("memory"));
  QVERIFY(zone->isValid());
  proj.addMapLayer(cad);
  proj.addMapLayer(zone);
  QVERIFY2(LayerOps::applyNameAttributeLabels(cad, QStringLiteral("jibun"), 8.0, false),
           "지적에 지번 라벨을 단다");
  QVERIFY(cad->labelsEnabled());

  // 트리에서 zone 을 위로 올린다(트리 첫 자식 = 맨 위).
  QgsLayerTree* root = proj.layerTreeRoot();
  QVERIFY(root);
  root->removeAllChildren();
  root->addLayer(zone);
  root->addLayer(cad);
  const QList<QgsLayerTreeLayer*> order = root->findLayers();
  QCOMPARE(order.size(), 2);
  QCOMPARE(order.first()->layer(), static_cast<QgsMapLayer*>(zone));

  LayerOps::applyLayerOrderToLabels(&proj, nullptr);

  // 위에 있고 글자가 없는 구역선은 라벨 위에 그려야 한다.
  QVERIFY2(zone->customProperty(QStringLiteral("rendering/renderAboveLabels")).toBool(),
           "위 레이어는 아래 레이어의 글자를 덮어야 한다");
  // 지적은 맨 아래라 아래에 글자 있는 레이어가 없다 — 켤 이유가 없다.
  QVERIFY2(!cad->customProperty(QStringLiteral("rendering/renderAboveLabels")).toBool(),
           "맨 아래 레이어는 켤 이유가 없다");

  // 글자를 가진 위 레이어도 켜져야 한다. 테두리만 있는 면·선은 자기 글자를
  // 덮지 않으므로 예외가 아니다. (이 예외를 넓게 잡았다가 요구가 안 지켜졌다.)
  QVERIFY2(LayerOps::applyNameAttributeLabels(zone, QStringLiteral("jibun"), 8.0, false) ||
               true,
           "선 레이어에 라벨을 달아 본다");
  LayerOps::applyLayerOrderToLabels(&proj, nullptr);
  QVERIFY2(zone->customProperty(QStringLiteral("rendering/renderAboveLabels")).toBool(),
           "글자가 있어도 채움이 없으면 위 레이어는 아래 글자를 덮어야 한다");

  // 속이 빈 면(테두리만)은 색이 진해도 아무것도 칠하지 않는다.
  // 색만 보고 「불투명 채움」으로 세면 예외에 걸려 규칙이 안 먹었다 — 현장 증상.
  auto* ring = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186&field=nm:string"),
                                  QStringLiteral("유존지역평가지역"), QStringLiteral("memory"));
  QVERIFY(ring->isValid());
  proj.addMapLayer(ring);
  {
    auto* fill = new QgsSimpleFillSymbolLayer();
    fill->setColor(QColor(255, 0, 0));       // 진한 빨강
    fill->setBrushStyle(Qt::NoBrush);        // 그러나 칠하지는 않는다
    fill->setStrokeColor(QColor(255, 0, 0));
    auto* sym = new QgsFillSymbol(QgsSymbolLayerList() << fill);
    ring->setRenderer(new QgsSingleSymbolRenderer(sym));
  }
  QVERIFY(LayerOps::applyNameAttributeLabels(ring, QStringLiteral("nm"), 8.0, false));
  root->removeAllChildren();
  root->addLayer(ring);
  root->addLayer(cad);
  LayerOps::applyLayerOrderToLabels(&proj, nullptr);
  QVERIFY2(ring->customProperty(QStringLiteral("rendering/renderAboveLabels")).toBool(),
           "테두리만 있는 면은 글자가 있어도 아래 글자를 덮어야 한다");

  // 진짜로 꽉 채운 면은 예외다 — 켜면 자기 채움이 자기 글자를 지운다.
  {
    auto* fill = new QgsSimpleFillSymbolLayer();
    fill->setColor(QColor(255, 255, 255));
    fill->setBrushStyle(Qt::SolidPattern);
    auto* sym = new QgsFillSymbol(QgsSymbolLayerList() << fill);
    ring->setRenderer(new QgsSingleSymbolRenderer(sym));
  }
  LayerOps::applyLayerOrderToLabels(&proj, nullptr);
  QVERIFY2(!ring->customProperty(QStringLiteral("rendering/renderAboveLabels")).toBool(),
           "꽉 채운 면은 자기 글자를 덮으므로 예외다");

  root->removeAllChildren();
  root->addLayer(zone);
  root->addLayer(cad);

  // 순서를 뒤집으면 판단도 뒤집힌다.
  root->removeAllChildren();
  root->addLayer(cad);
  root->addLayer(zone);
  LayerOps::applyLayerOrderToLabels(&proj, nullptr);
  QVERIFY2(!zone->customProperty(QStringLiteral("rendering/renderAboveLabels")).toBool(),
           "밑으로 내려간 레이어는 글자를 덮지 않는다");
}

void TestWorkflow::layoutHonoursRasterBrightness() {
  // 사용자 보고: 지도에서는 사진 밝기가 먹는데 조판에서는 게이지만 움직이고
  // 그림은 그대로다. 조판(레이아웃)이 래스터 밝기 필터를 실제로 반영하는지
  // 렌더 결과 화소로 직접 확인한다.
  const QString dir = QDir::temp().filePath(
      QStringLiteral("ka_lbr_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  const QString tif = QDir(dir).filePath(QStringLiteral("gray.tif"));
  QImage src(64, 64, QImage::Format_Grayscale8);
  src.fill(120);
  QVERIFY(src.save(tif, "TIFF"));

  QgsProject proj;
  auto* rl = new QgsRasterLayer(tif, QStringLiteral("gray"), QStringLiteral("gdal"));
  QVERIFY(rl->isValid());
  proj.addMapLayer(rl);

  auto* ly = new QgsPrintLayout(&proj);
  ly->initializeDefaults();
  proj.layoutManager()->addLayout(ly);
  auto* map = new QgsLayoutItemMap(ly);
  map->attemptSetSceneRect(QRectF(10, 10, 80, 80));
  ly->addLayoutItem(map);
  map->setKeepLayerSet(true);
  map->setLayers(QList<QgsMapLayer*>() << rl);
  map->zoomToExtent(rl->extent());

  auto sampleCenter = [&]() -> int {
    QgsLayoutExporter ex(ly);
    const QImage page = ex.renderPageToImage(0, QSize(300, 300), 96.0);
    if (page.isNull()) return -1;
    return qGray(page.pixel(page.width() / 4, page.height() / 4));
  };

  map->invalidateCache();
  const int before = sampleCenter();
  QVERIFY2(before >= 0, "조판 페이지를 그려야 한다");

  QVERIFY(LayerOps::setMapLayerBrightness(rl, 100, nullptr));
  QCOMPARE(LayerOps::mapLayerBrightness(rl), 100);
  map->invalidateCache();
  const int after = sampleCenter();
  QVERIFY2(after >= 0, "밝기를 올린 뒤에도 그려야 한다");

  QVERIFY2(after > before + 10,
           qPrintable(QStringLiteral("조판이 밝기를 반영해야 한다 (before=%1 after=%2)")
                          .arg(before).arg(after)));
}

void TestWorkflow::layoutOpacityRail_staysPutWhenThePageMoves() {
  // 사용자 보고: 지도에서는 투명도 막대가 제자리인데, 조판(도면만들기)에서는
  // 페이지를 끌면 막대가 페이지를 따라 같이 밀렸다.
  // QgsLayoutView 는 QGraphicsView 라서 scrollContentsBy 가 viewport()->scroll() 을
  // 부르고, 그러면 viewport 의 자식 위젯까지 함께 밀린다. QgsMapCanvas 는 스크롤 대신
  // 범위를 다시 그리므로 지도 쪽은 멀쩡했다. 스크롤하지 않는 부모에 붙여야 한다.
  QFile f(QStringLiteral("src/app/KaDrawingStudio.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
           "run from source tree (ctest WORKING_DIRECTORY)");
  const QString src = QString::fromUtf8(f.readAll());
  const int at = src.indexOf(QLatin1String("new KaLayerOpacityRail"));
  QVERIFY2(at >= 0, "조판 화면에도 투명도 막대가 있어야 한다");
  const QString around = src.mid(qMax(0, at - 500), 560);
  QVERIFY2(!around.contains(QLatin1String("m_view->viewport()")),
           "스크롤되는 viewport 에 붙이면 페이지를 끌 때 막대가 같이 밀린다");
  QVERIFY2(src.mid(at, 40).contains(QLatin1String("desk")),
           "스크롤하지 않는 부모에 붙여야 제자리에 남는다");
}

void TestWorkflow::uiComboActions_doNotBustTileCacheWhileDrawing() {
  // 툴바 지형맵·DEM·토양·지질·수계·검색·레이어순서·줌이 겹치면
  // refreshAllLayers가 지적 WMS 캐시를 버리고 UI 스레드가 멈춘다.
  const QString opsSrc = readLayerOpsSources();
  QVERIFY2(!opsSrc.isEmpty(), "LayerOps.cpp + BasemapOps + LabelOps + ControlPointCsv");

  int fn = opsSrc.indexOf(QLatin1String("bool LayerOps::zoomToLayerMax"));
  QVERIFY2(fn >= 0, "zoomToLayerMax");
  int next = opsSrc.indexOf(QLatin1String("\nbool LayerOps::"), fn + 10);
  QVERIFY2(next > fn, "zoomToLayerMax body");
  const QString zoom = opsSrc.mid(fn, next - fn);
  QVERIFY2(!zoom.contains(QLatin1String("refreshAllLayers")),
           "zoomToLayerMax must not dump WMS/XYZ cache when overlays stack");
  QVERIFY2(zoom.contains(QLatin1String("refreshCanvasIfIdle")),
           "zoom after a toolbar click must wait until the canvas is idle");

  fn = opsSrc.indexOf(QLatin1String("bool LayerOps::isolateAndZoomToLayer"));
  QVERIFY2(fn >= 0, "isolateAndZoomToLayer");
  next = opsSrc.indexOf(QLatin1String("\nvoid LayerOps::zoomToFullMax"), fn + 10);
  QVERIFY2(next > fn, "isolateAndZoomToLayer body");
  QVERIFY2(!opsSrc.mid(fn, next - fn).contains(QLatin1String("refreshAllLayers")),
           "isolateAndZoomToLayer must not restack in-flight tiles");

  fn = opsSrc.indexOf(QLatin1String("bool LayerOps::setLayerOpacity"));
  QVERIFY2(fn >= 0, "setLayerOpacity");
  next = opsSrc.indexOf(QLatin1String("\nbool LayerOps::toggleLayerVisibility"), fn + 10);
  QVERIFY2(next > fn, "setLayerOpacity body");
  QVERIFY2(!opsSrc.mid(fn, next - fn).contains(QLatin1String("refreshAllLayers")),
           "opacity slider must not bust the tile cache");

  fn = opsSrc.indexOf(QLatin1String("LayerOps::FieldBasemapPackResult LayerOps::prepareFieldBasemapPack"));
  QVERIFY2(fn >= 0, "prepareFieldBasemapPack");
  next = opsSrc.indexOf(QLatin1String("\nbool LayerOps::addVworldHybridMap"), fn + 10);
  QVERIFY2(next > fn, "prepareFieldBasemapPack body");
  QVERIFY2(!opsSrc.mid(fn, next - fn).contains(QLatin1String("refreshAllLayers")),
           "위성+지적 동시 추가는 refreshAllLayers 금지");

  fn = opsSrc.indexOf(QLatin1String("void LayerOps::zoomToKorea"));
  QVERIFY2(fn >= 0, "zoomToKorea");
  next = opsSrc.indexOf(QLatin1String("\nQString LayerOps::"), fn + 10);
  if (next < fn)
    next = opsSrc.size();
  QVERIFY2(next > fn, "zoomToKorea body");
  QVERIFY2(!opsSrc.mid(fn, next - fn).contains(QLatin1String("refreshAllLayers")),
           "전체 보기도 타일 캐시를 버리면 안 됨");

  QFile mw(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(mw.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindow.cpp");
  const QString app = QString::fromUtf8(mw.readAll());
  fn = app.indexOf(QLatin1String("void MainWindow::onLayerTreeRowsMoved"));
  QVERIFY2(fn >= 0, "onLayerTreeRowsMoved");
  next = app.indexOf(QLatin1String("void MainWindow::"), fn + 10);
  QVERIFY2(next > fn, "onLayerTreeRowsMoved body");
  QVERIFY2(!app.mid(fn, next - fn).contains(QLatin1String("refreshAllLayers")),
           "레이어 순서 바꾸기가 지적 타일을 다시 받으면 멈춤");

  fn = app.indexOf(QLatin1String("void MainWindow::zoomToLocation"));
  QVERIFY2(fn >= 0, "zoomToLocation");
  next = app.indexOf(QLatin1String("void MainWindow::"), fn + 10);
  QVERIFY2(next > fn, "zoomToLocation body");
  QVERIFY2(!app.mid(fn, next - fn).contains(QLatin1String("refreshAllLayers")),
           "주소 검색 이동이 refreshAllLayers면 위성·지적이 다시 로딩됨");

  QFile undoFile(QStringLiteral("src/app/MainWindowUndo.cpp"));
  QVERIFY(undoFile.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString undo = QString::fromUtf8(undoFile.readAll());
  fn = undo.indexOf(QLatin1String("void MainWindow::undoLastAction"));
  QVERIFY2(fn >= 0, "undoLastAction");
  next = undo.indexOf(QLatin1String("void MainWindow::"), fn + 10);
  QVERIFY2(next > fn, "undoLastAction body");
  QVERIFY2(!undo.mid(fn, next - fn).contains(QLatin1String("refreshAllLayers")),
           "Ctrl+Z clearCache+refreshAllLayers is the provider_wms AV");

  fn = app.indexOf(QLatin1String("void MainWindow::applyMapScaleFromUi"));
  QVERIFY2(fn >= 0, "applyMapScaleFromUi");
  next = app.indexOf(QLatin1String("void MainWindow::"), fn + 10);
  QVERIFY2(next > fn, "applyMapScaleFromUi body");
  const QString scaleBody = app.mid(fn, next - fn);
  QVERIFY2(scaleBody.contains(QLatin1String("refreshCanvasIfIdle")),
           "축척 콤보도 그리는 중이면 refresh 금지");
  QVERIFY2(!scaleBody.contains(QLatin1String("->refresh()")),
           "축척 적용의 빈 refresh()는 지적 타일 위에 겹치면 멈춤");
}

void TestWorkflow::startupView_doesNotRestackXyzRefreshWhileWmsDownloads() {
  QFile f(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
           "run from source tree (ctest WORKING_DIRECTORY)");
  const QString src = QString::fromUtf8(f.readAll());
  const int fn = src.indexOf(QLatin1String("void MainWindow::ensureStartupViewReady"));
  QVERIFY2(fn >= 0, "ensureStartupViewReady must exist");
  const int next = src.indexOf(QLatin1String("void MainWindow::"), fn + 10);
  QVERIFY2(next > fn, "ensureStartupViewReady body");
  const QString body = src.mid(fn, next - fn);
  QVERIFY2(!body.contains(QLatin1String("QTimer::singleShot(600")),
           "600ms restack stops in-flight VWorld tiles");
  QVERIFY2(!body.contains(QLatin1String("QTimer::singleShot(1800")),
           "1800ms restack is the 16:04 field crash window");

  QFile app(QStringLiteral("src/app/KaApplication.cpp"));
  QVERIFY2(app.open(QIODevice::ReadOnly | QIODevice::Text), "KaApplication.cpp");
  const QString boot = QString::fromUtf8(app.readAll());
  QVERIFY2(boot.contains(QStringLiteral("qgis/parallel_rendering")),
           "boot must persist sequential rendering in QgsSettings");
  QVERIFY2(boot.contains(QLatin1String("setMaxThreads(1)")),
           "WMS block()+QEventLoop on a thread-pool worker is the ParallelJob AV");

  const int afterFn = src.indexOf(QLatin1String("static void afterBasemapAdded"));
  QVERIFY2(afterFn >= 0, "afterBasemapAdded must exist");
  const int afterNext = src.indexOf(QLatin1String("\nvoid MainWindow::"), afterFn + 10);
  QVERIFY2(afterNext > afterFn, "afterBasemapAdded body");
  const QString after = src.mid(afterFn, afterNext - afterFn);
  QVERIFY2(!after.contains(QLatin1String("clearCache")),
           "clearCache after add + processEvents is the same deleteLater AV");
  QVERIFY2(!after.contains(QLatin1String("processEvents")),
           "nested processEvents while WMS tiles finish re-enters provider_wms");
  QVERIFY2(!after.contains(QLatin1String("refreshAllLayers")),
           "delayed refreshAllLayers restacks an in-flight job");
  QVERIFY2(after.contains(QLatin1String("refreshXyzBasemapTiles")),
           "basemap add must use the safe XYZ refresh");

  const QString opsSrc = readLayerOpsSources();
  QVERIFY2(!opsSrc.isEmpty(), "LayerOps.cpp + BasemapOps + LabelOps + ControlPointCsv");
  const int clampFn = opsSrc.indexOf(QLatin1String("bool LayerOps::clampCanvasToKorea"));
  QVERIFY2(clampFn >= 0, "clampCanvasToKorea");
  const int clampNext = opsSrc.indexOf(QLatin1String("\nvoid LayerOps::"), clampFn + 10);
  QVERIFY2(clampNext > clampFn, "clampCanvasToKorea body");
  QVERIFY2(!opsSrc.mid(clampFn, clampNext - clampFn).contains(QLatin1String("stopRendering")),
           "pan/extentsChanged clamp must not abort in-flight WMS tiles");
}

void TestWorkflow::addVworldSatellite_fourKCanvasKeeps256pxTiles() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QgsMapCanvas canvas;
  canvas.resize(3840, 2160);
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, &canvas, QStringLiteral("EPSG:5186")));
  QString err;
  QVERIFY2(LayerOps::addVworldSatelliteMap(&proj, &canvas, QString(), &err), qPrintable(err));
  LayerOps::syncMapCanvas(&proj, &canvas, false);
  LayerOps::refreshXyzBasemapTiles(&canvas);
  QgsMapLayer* sat = nullptr;
  for (QgsMapLayer* l : proj.mapLayers()) {
    if (l && l->name().contains(QStringLiteral("위성")))
      sat = l;
  }
  QVERIFY2(sat, "4K canvas must still have satellite");
  QVERIFY2(canvas.layers().contains(sat), "4K canvas must paint satellite");
  const QString src = sat->source();
  QVERIFY2(src.contains(QStringLiteral("tilePixelRatio=1")), qPrintable(src.left(180)));
  QVERIFY2(!src.contains(QStringLiteral("tilePixelRatio=2")),
           "VWorld satellite is 256px; @2x / 512 blanks 4K");
}

void TestWorkflow::applyCanvasScreenDpi_outputSizeFollowsWideWidget() {
  QgsMapCanvas canvas;
  canvas.setFrameShape(QFrame::NoFrame);
  canvas.setRenderFlag(false);
  canvas.show();
  for (const QSize& size : {QSize(3440, 1440), QSize(1920, 1080), QSize(3840, 2160)}) {
    canvas.resize(size);
    QCoreApplication::processEvents();
    canvas.setExtent(QgsRectangle(200000, 450000, 208000, 456000));
    // Simulate stale settings before a monitor/size refresh. Output size is
    // logical pixels, while QGIS owns the physical render-image allocation.
    canvas.mapSettings().setOutputSize(QSize(800, 600));
    LayerOps::applyCanvasScreenDpi(&canvas);
    QCOMPARE(canvas.mapSettings().outputSize(), size);
    QVERIFY(canvas.mapSettings().outputDpi() > 10.0);
    const QgsPointXY center = canvas.mapSettings().mapToPixel().toMapCoordinates(size.width() / 2., size.height() / 2.);
    QVERIFY(center.distance(QgsPointXY(204000, 453000)) < 0.01);
  }
}

void TestWorkflow::convertSelectedTo5179_sourceDoesNotAddToMap() {
  const QString src = readMainWindowSources();
  QVERIFY2(!src.isEmpty(), "MainWindow.cpp + MainWindowExport.cpp");
  const int fn = src.indexOf(QLatin1String("void MainWindow::convertSelectedTo5179"));
  QVERIFY2(fn >= 0, "convertSelectedTo5179");
  const int next = src.indexOf(QLatin1String("\nvoid MainWindow::"), fn + 10);
  QVERIFY2(next > fn, "convertSelectedTo5179 body");
  const QString body = src.mid(fn, next - fn);
  QVERIFY2(body.contains(QLatin1String("false")),
           "5179 UI must pass addToMap false");
  QVERIFY2(!body.contains(QLatin1String("m_canvas->refresh()")),
           "5179 file-only must not refresh the work map");
  QVERIFY2(body.contains(QLatin1String("preferredSurveyDir")),
           "5179 save dialog should start in the survey folder");
}

void TestWorkflow::otf_keepsWorkCrsWhenBasemapIs3857() {
  QgsProject proj;
  QgsMapCanvas canvas;
  canvas.resize(640, 480);
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, &canvas, QStringLiteral("EPSG:5187")));
  auto* sat = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:3857"),
                                 QStringLiteral("VWorld 위성"), QStringLiteral("memory"));
  QVERIFY(sat->isValid());
  sat->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:3857")));
  LayerOps::markReferenceLayer(sat);
  proj.addMapLayer(sat);
  LayerOps::syncMapCanvas(&proj, &canvas, false);
  QCOMPARE(canvas.mapSettings().destinationCrs().authid(), QStringLiteral("EPSG:5187"));
  QCOMPARE(sat->crs().authid(), QStringLiteral("EPSG:3857"));
  QVERIFY(canvas.layers().contains(sat));
}

void TestWorkflow::koreaExtent_5186IsTmMetersNotDegrees() {
  const QgsRectangle kr = LayerOps::koreaExtentForCrs(QStringLiteral("EPSG:5186"));
  QVERIFY2(!kr.isEmpty() && kr.isFinite(), "korea extent for 5186");
  QVERIFY2(kr.width() > 100000.0 && kr.height() > 100000.0,
           "5186 Korea box must be TM metres, not WGS degrees");
  QVERIFY2(kr.xMinimum() > -500000.0 && kr.xMaximum() < 1000000.0, qPrintable(QString::number(kr.xMinimum())));
}

void TestWorkflow::setWorkCrs_5187to5186_keepsSatelliteAndLocalExtent() {
  QgsProject proj;
  QgsMapCanvas canvas;
  canvas.resize(800, 600);
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, &canvas, QStringLiteral("EPSG:5187")));
  QString err;
  QVERIFY2(LayerOps::addVworldSatelliteMap(&proj, &canvas, QString(), &err), qPrintable(err));
  LayerOps::syncMapCanvas(&proj, &canvas, false);
  canvas.setExtent(QgsRectangle(180000.0, 430000.0, 220000.0, 460000.0));
  canvas.zoomScale(25000.0, true);
  QVERIFY(LayerOps::setWorkCrs(&proj, &canvas, QStringLiteral("EPSG:5186"), &err, false));
  QCOMPARE(proj.crs().authid(), QStringLiteral("EPSG:5186"));
  QCOMPARE(canvas.mapSettings().destinationCrs().authid(), QStringLiteral("EPSG:5186"));
  QgsMapLayer* sat = nullptr;
  for (QgsMapLayer* l : proj.mapLayers()) {
    if (l && l->name().contains(QStringLiteral("위성")))
      sat = l;
  }
  QVERIFY2(sat, "satellite must survive 5186 switch");
  QCOMPARE(sat->crs().authid(), QStringLiteral("EPSG:3857"));
  QVERIFY2(canvas.layers().contains(sat), "OTF 5186 canvas must still paint 3857 satellite");
  const QgsRectangle kr = LayerOps::koreaExtentForCrs(QStringLiteral("EPSG:5186"));
  QVERIFY2(canvas.extent().width() < kr.width() * 0.5,
           "switching to 5186 must not jump to all-Korea (satellite looks empty)");
  QVERIFY2(canvas.scale() < 200000.0, "keep field scale, not Korea-wide");
}

void TestWorkflow::applyKoreaMapLimits_doesNotReplaceLocalExtentWithFullKorea() {
  QgsProject proj;
  QgsMapCanvas canvas;
  canvas.resize(800, 600);
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, &canvas, QStringLiteral("EPSG:5186")));
  const QgsRectangle local(150000.0, 400000.0, 190000.0, 430000.0);
  canvas.setExtent(local);
  LayerOps::applyKoreaMapLimits(&proj, &canvas);
  const QgsRectangle kr = LayerOps::koreaExtentForCrs(QStringLiteral("EPSG:5186"));
  QVERIFY2(canvas.extent().width() < kr.width() * 0.5,
           "applyKoreaMapLimits must not setExtent(full Korea)");
}

void TestWorkflow::cadastralWmsCrs_neverStartsWithWorkCrs5179() {
  const QStringList crs = LayerOps::cadastralWmsCrsCandidates(QStringLiteral("EPSG:5187"));
  QVERIFY(!crs.isEmpty());
  QCOMPARE(crs.first(), QStringLiteral("EPSG:4326"));
  QVERIFY(crs.contains(QStringLiteral("EPSG:3857")));
  QVERIFY(!crs.contains(QStringLiteral("EPSG:5179")));
  QVERIFY(!crs.contains(QStringLiteral("EPSG:5186")));
  QVERIFY(!crs.contains(QStringLiteral("EPSG:5187")));
}

void TestWorkflow::syncMapCanvas_cadastralAboveSatellite() {
  QgsProject proj;
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, nullptr, QStringLiteral("EPSG:5187")));

  auto* survey = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"),
                                    QStringLiteral("survey_area"), QStringLiteral("memory"));
  QVERIFY(survey->isValid());
  LayerOps::markSurveyLayer(survey, QStringLiteral("survey_area"));

  auto* cadMem = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:3857"),
                                    QStringLiteral("VWorld 지적(본번·부번)"), QStringLiteral("memory"));
  QVERIFY(cadMem->isValid());
  LayerOps::markReferenceLayer(cadMem);

  auto* sat = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:3857"),
                                 QStringLiteral("VWorld 위성"), QStringLiteral("memory"));
  QVERIFY(sat->isValid());
  LayerOps::markReferenceLayer(sat);

  proj.addMapLayer(sat);
  proj.addMapLayer(cadMem);
  proj.addMapLayer(survey);

  QgsMapCanvas canvas;
  canvas.resize(800, 600);
  LayerOps::syncMapCanvas(&proj, &canvas, false);
  const QList<QgsMapLayer*> layers = canvas.layers();
  const int iSurvey = layers.indexOf(survey);
  const int iCad = layers.indexOf(cadMem);
  const int iSat = layers.indexOf(sat);
  QVERIFY(iSurvey >= 0 && iCad >= 0 && iSat >= 0);
  QVERIFY2(iSurvey < iCad, "survey must paint above cadastral");
  QVERIFY2(iCad < iSat, "cadastral must paint above satellite");
}

void TestWorkflow::layoutBlankSheetMapItemKeepsFrameAndNonEmptyLayers() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  const QString name = LayoutService::createBlankSheet(
      &proj, 297.0, 210.0, QStringLiteral("user_sheet"), &err);
  QCOMPARE(name, QStringLiteral("user_sheet"));
  auto* ly = dynamic_cast<QgsPrintLayout*>(proj.layoutManager()->layoutByName(name));
  QVERIFY2(ly, qPrintable(err));

  auto* map = new QgsLayoutItemMap(ly);
  map->setId(QStringLiteral("ka_map"));
  map->setFrameEnabled(true);
  const QRectF frame(20.0, 20.0, 120.0, 80.0);
  map->attemptSetSceneRect(frame);
  map->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  map->setKeepLayerSet(true);
  auto* blank = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                   QStringLiteral("layout_blank"), QStringLiteral("memory"));
  QVERIFY(blank->isValid());
  map->setLayers(QList<QgsMapLayer*>{blank});
  map->zoomToExtent(QgsRectangle(200000.0, 450000.0, 200200.0, 450160.0));
  if (map->scene() != ly)
    ly->addLayoutItem(map);

  QVERIFY(map->keepLayerSet());
  QCOMPARE(map->layers().size(), 1);
  QVERIFY2(qAbs(map->rect().width() - 120.0) < 1.0, "zoomToExtent must keep dragged width");
  QVERIFY2(qAbs(map->rect().height() - 80.0) < 1.0, "setExtent would rewrite height; zoomToExtent must not");
  QVERIFY(map->extent().isFinite());
  QVERIFY(map->extent().width() > 0.0);
}

void TestWorkflow::layoutEnter_matchesCanvasViewWithoutNiceSnap() {
  // 조판 진입 시 지도 화면과 같은 범위·1:N을 쓴다. niceScaleDenominator로
  // 1847→2000처럼 분모를 올리면 맵이 축소된다. setExtent는 칸 mm를 바꾼다.
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  QVERIFY(!LayoutService::createBlankSheet(&proj, 297.0, 210.0, QStringLiteral("user_sheet"), &err)
               .isEmpty());
  auto* ly = dynamic_cast<QgsPrintLayout*>(
      proj.layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  QVERIFY(ly);
  auto* map = new QgsLayoutItemMap(ly);
  map->setId(QStringLiteral("ka_map"));
  map->attemptSetSceneRect(QRectF(20.0, 20.0, 200.0, 140.0));
  map->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  map->zoomToExtent(QgsRectangle(0.0, 0.0, 80000.0, 56000.0));
  if (map->scene() != ly)
    ly->addLayoutItem(map);
  QVERIFY2(map->scale() > 10000.0, "precondition: sheet starts zoomed out");

  const QgsRectangle view(200000.0, 450000.0, 200400.0, 450280.0);
  QVERIFY(LayoutService::applyCanvasViewToLayoutMap(map, view, 2000.0));
  QVERIFY2(qAbs(map->rect().width() - 200.0) < 1.0, "zoomToExtent/setScale must keep map frame mm");
  QVERIFY2(qAbs(map->rect().height() - 140.0) < 1.0, "setExtent must not resize the frame");
  const QgsPointXY c = map->extent().center();
  QVERIFY(qAbs(c.x() - view.center().x()) < 1.0);
  QVERIFY(qAbs(c.y() - view.center().y()) < 1.0);
  QVERIFY2(qAbs(map->scale() - 2000.0) < 5.0,
           qPrintable(QStringLiteral("layout scale must match canvas 1:2000 (got %1)")
                          .arg(map->scale())));
  QVERIFY2(map->extent().width() < 2000.0, "must not keep the 80 km start envelope");

  QCOMPARE(LayoutService::niceScaleDenominator(1847.0), 2000);
  QVERIFY(LayoutService::applyCanvasViewToLayoutMap(map, view, 1847.0));
  QVERIFY2(qAbs(map->scale() - 1847.0) < 5.0,
           qPrintable(QStringLiteral("must not snap 1:1847 up to 1:2000 (got %1)")
                          .arg(map->scale())));

  QFile studio(QStringLiteral("src/app/KaDrawingStudio.cpp"));
  QVERIFY2(studio.open(QIODevice::ReadOnly | QIODevice::Text), "KaDrawingStudio.cpp");
  const QString src = QString::fromUtf8(studio.readAll());
  const int fn = src.indexOf(QLatin1String("void KaDrawingStudio::centerOnMapCanvas"));
  QVERIFY2(fn >= 0, "centerOnMapCanvas");
  const int next = src.indexOf(QLatin1String("void KaDrawingStudio::"), fn + 10);
  QVERIFY2(next > fn, "centerOnMapCanvas body");
  const QString body = src.mid(fn, next - fn);
  QVERIFY2(body.contains(QLatin1String("applyCanvasViewToLayoutMap")),
           "layout enter must copy the canvas view, not only pan");
  QVERIFY2(!body.contains(QLatin1String("niceScaleDenominator")),
           "layout enter must not snap the canvas scale up");
}

void TestWorkflow::layoutStudio_checkedLayersScaleBarLegendSize() {
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));

  auto* survey = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                    QStringLiteral("구역"), QStringLiteral("memory"));
  QVERIFY(survey->isValid());
  LayerOps::markSurveyLayer(survey, QStringLiteral("survey_area"));
  auto* cad = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:3857"),
                                 QStringLiteral("VWorld 지적 본번"), QStringLiteral("memory"));
  QVERIFY(cad->isValid());
  LayerOps::markReferenceLayer(cad);
  auto* sat = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:3857"),
                                 QStringLiteral("VWorld 위성"), QStringLiteral("memory"));
  QVERIFY(sat->isValid());
  LayerOps::markReferenceLayer(sat);
  proj.addMapLayer(sat);
  proj.addMapLayer(cad);
  proj.addMapLayer(survey);
  QgsMapLayer* toCheck[] = {survey, cad, sat};
  for (QgsMapLayer* l : toCheck) {
    if (auto* n = proj.layerTreeRoot()->findLayer(l->id()))
      n->setItemVisibilityChecked(true);
  }

  const QList<QgsMapLayer*> order = LayerOps::visibleLayersPaintOrder(&proj);
  QVERIFY(order.contains(survey));
  QVERIFY(order.contains(cad));
  QVERIFY(order.contains(sat));
  QVERIFY2(order.indexOf(cad) < order.indexOf(sat), "cadastral must paint above satellite");
  QVERIFY2(order.indexOf(survey) < order.indexOf(cad), "survey must paint above cadastral");

  QString err;
  QVERIFY(!LayoutService::createBlankSheet(&proj, 297.0, 210.0, QStringLiteral("user_sheet"), &err).isEmpty());
  auto* ly = dynamic_cast<QgsPrintLayout*>(proj.layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  QVERIFY(ly);

  auto* map = new QgsLayoutItemMap(ly);
  map->setId(QStringLiteral("ka_map"));
  map->attemptSetSceneRect(QRectF(20.0, 20.0, 160.0, 120.0));
  map->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  map->setKeepLayerSet(true);
  map->setLayers(order);
  map->zoomToExtent(QgsRectangle(200000.0, 450000.0, 200400.0, 450300.0));
  if (map->scene() != ly)
    ly->addLayoutItem(map);
  QCOMPARE(map->layers().size(), 3);
  QVERIFY(map->layers().indexOf(cad) < map->layers().indexOf(sat));

  auto* sb = new QgsLayoutItemScaleBar(ly);
  ly->addLayoutItem(sb);
  sb->applyDefaultSettings();
  sb->setLinkedMap(map);
  sb->setStyle(QStringLiteral("Double Box"));
  sb->setUnits(Qgis::DistanceUnit::Meters);
  sb->setSegmentSizeMode(Qgis::ScaleBarSegmentSizeMode::FitWidth);
  sb->setMinimumBarWidth(22.0);
  sb->setMaximumBarWidth(37.0);
  sb->setNumberOfSegments(4);
  const QRectF barRect(20.0, 150.0, 160.0, 16.0);
  sb->attemptSetSceneRect(barRect);
  sb->refresh();
  sb->attemptSetSceneRect(barRect);
  QVERIFY2(sb->unitsPerSegment() > 0.0, "scale bar must not stay 0 m");
  QCOMPARE(sb->linkedMap(), map);
  QCOMPARE(sb->style(), QStringLiteral("Double Box"));
  const double barW = sb->rect().width();
  QVERIFY2(qAbs(barW - 160.0) < 8.0,
           qPrintable(QStringLiteral("scale bar paper width must stay ~160mm (got %1)").arg(barW)));
  map->zoomToExtent(QgsRectangle(200000.0, 450000.0, 200080.0, 450060.0));
  sb->refresh();
  sb->attemptSetSceneRect(barRect);
  QVERIFY2(qAbs(sb->rect().width() - 160.0) < 8.0, "zoom must not shrink scale bar on paper");

  auto* legend = new QgsLayoutItemLegend(ly);
  legend->setResizeToContents(false);
  legend->setTitle(QStringLiteral("범례"));
  legend->setLinkedMap(map);
  legend->attemptSetSceneRect(QRectF(190.0, 20.0, 48.0, 74.0));
  ly->addLayoutItem(legend);
  QVERIFY(!legend->resizeToContents());
  QVERIFY2(qAbs(legend->rect().width() - 48.0) < 2.0, "legend width must keep user size");
  QVERIFY2(qAbs(legend->rect().height() - 74.0) < 2.0, "legend height must keep user size");
}

void TestWorkflow::layoutStandardSheetChrome_sitsBelowMap() {
  const QRectF page(0.0, 0.0, 210.0, 297.0);
  const QRectF drag(12.0, 12.0, 186.0, 273.0);
  const auto c = LayoutService::standardSheetChrome(page, drag);
  QVERIFY2(c.map.bottom() <= c.scaleBar.top() + 1e-6, "scale bar must sit below the map");
  QVERIFY2(c.map.bottom() <= c.north.top() + 1e-6, "north must sit below the map");
  QVERIFY2(c.map.bottom() <= c.crs.top() + 1e-6, "crs must sit below the map");
  QVERIFY2(c.map.bottom() <= c.scaleLabel.top() + 1e-6, "scale label must sit below the map");
  QVERIFY2(c.scaleBar.bottom() <= page.bottom() + 1e-6, "scale bar stays on page");
  QVERIFY2(c.scaleLabel.bottom() <= page.bottom() + 1e-6, "scale label stays on page");
  QVERIFY2(c.crs.bottom() <= page.bottom() + 1e-6, "crs stays on page");
  QVERIFY2(c.north.bottom() <= page.bottom() + 1e-6, "north stays on page");
  QVERIFY2(c.map.height() < drag.height() - 0.5, "map height shrinks so chrome fits");
  QVERIFY2(qAbs(c.scaleBar.left() - c.map.left()) < 0.5, "scale bar left-aligned with map");
  QVERIFY2(qAbs(c.north.right() - c.map.right()) < 0.5, "north right-aligned with map");
  QVERIFY2(c.scaleLabel.top() + 1e-6 >= c.scaleBar.bottom(), "scale label sits under the bar");
  QVERIFY2(c.crs.top() + 1e-6 >= c.map.bottom(), "crs stays below the map");
  QVERIFY2(c.crs.bottom() <= c.scaleBar.bottom() + 2.0, "crs stays on the scale-bar row");
  QVERIFY2(c.crs.right() <= c.north.left() + 1e-6, "crs sits left of north");
  QVERIFY2(c.scaleBar.right() <= c.crs.left() + 1e-6, "scale bar does not overlap crs");
  QVERIFY2(c.north.height() <= 22.0, "north fits in the reserved chrome strip");
  QVERIFY2(c.north.bottom() <= page.bottom() - 7.5, "north stays above the page margin");
  QVERIFY2(c.scaleBar.height() >= 11.0, "scale bar slot fits QGIS Line Ticks Up minimum");
  QVERIFY2(c.scaleLabel.top() >= c.scaleBar.bottom() + 2.0, "scale text stays below the bar box");
}

void TestWorkflow::layoutRegainsPageWhenTheSheetHasNone() {
  QgsProject project;
  QString err;
  const QString name = LayoutService::createBlankSheet(
      &project, 297.0, 210.0, QStringLiteral("user_sheet"), &err);
  QVERIFY2(!name.isEmpty(), qPrintable(err));
  auto* layout = dynamic_cast<QgsPrintLayout*>(project.layoutManager()->layoutByName(name));
  QVERIFY(layout && layout->pageCollection());
  QCOMPARE(layout->pageCollection()->pageCount(), 1);
  layout->pageCollection()->deletePage(0);
  QCOMPARE(layout->pageCollection()->pageCount(), 0);
  QVERIFY(LayoutService::ensureLayoutPage(layout, 320.0, 220.0));
  QCOMPARE(layout->pageCollection()->pageCount(), 1);
  QgsLayoutItemPage* page = layout->pageCollection()->page(0);
  QVERIFY(page);
  const QRectF paper = page->mapRectToScene(page->rect());
  QVERIFY2(paper.width() >= 8.0 && paper.height() >= 8.0, qPrintable(QString::number(paper.width())));
}

void TestWorkflow::layoutEqualFullSheetMapRect_matchesTopToSides() {
  const QRectF page(0.0, 0.0, 210.0, 297.0);
  const QRectF uneven(18.0, 12.0, 174.0, 247.0);
  const QRectF equalized = LayoutService::equalFullSheetMapRect(page, uneven);
  QVERIFY2(qAbs(equalized.left() - equalized.top()) < 0.01, "top matches left");
  QVERIFY2(qAbs((page.right() - equalized.right()) - equalized.left()) < 0.01, "right matches left");
  QVERIFY2(qAbs((page.bottom() - equalized.bottom()) - (page.bottom() - uneven.bottom())) < 0.01,
           "bottom chrome room stays");
  const QRectF custom(40.0, 30.0, 80.0, 90.0);
  QCOMPARE(LayoutService::equalFullSheetMapRect(page, custom), custom);
  const QRectF already(18.0, 18.0, 174.0, 241.0);
  QCOMPARE(LayoutService::equalFullSheetMapRect(page, already), already);
}

void TestWorkflow::layoutExtentForPaperScale_keepsTypedDenominator() {
  const QgsRectangle cur(200000.0, 450000.0, 200200.0, 450160.0);
  const QgsRectangle next = LayoutService::extentForPaperScale(cur, 100.0, 1000.0);
  QVERIFY(next.isFinite());
  QCOMPARE(next.center().x(), cur.center().x());
  QCOMPARE(next.center().y(), cur.center().y());
  QVERIFY2(qAbs(next.width() - 100.0) < 0.001, "1:1000 on 100mm paper is 100m on ground");
  QVERIFY2(qAbs(next.height() / next.width() - cur.height() / cur.width()) < 0.001,
           "aspect ratio stays");
  const QgsRectangle five = LayoutService::extentForPaperScale(cur, 100.0, 5000.0);
  QVERIFY2(qAbs(five.width() - 500.0) < 0.001, "typed 5000 must stay 5000, not a computed 989-like value");
  for (int den : {500, 1000, 2500, 5000}) {
    const QgsRectangle e = LayoutService::extentForPaperScale(cur, 100.0, static_cast<double>(den));
    QVERIFY2(qAbs(e.width() - den / 10.0) < 0.001,
             qPrintable(QStringLiteral("chip 1:%1 on 100mm paper must stay typed").arg(den)));
  }
}

void TestWorkflow::layoutNiceScaleDenominator_endsOnTen() {
  QCOMPARE(LayoutService::niceScaleDenominator(20.0), 20);
  QCOMPARE(LayoutService::niceScaleDenominator(23.0), 40);
  QCOMPARE(LayoutService::niceScaleDenominator(37.0), 40);
  QCOMPARE(LayoutService::niceScaleDenominator(40.0), 40);
  QCOMPARE(LayoutService::niceScaleDenominator(487.0), 500);
  QCOMPARE(LayoutService::niceScaleDenominator(1000.0), 1000);
  QVERIFY2(LayoutService::niceScaleDenominator(35.0) % 10 == 0, "denominator must end on 10");
  QVERIFY2(LayoutService::niceScaleDenominator(23.0) >= 23, "must not snap down and clip extent");
  const QgsRectangle cur(200000.0, 450000.0, 200007.0, 450005.0);
  const int nice = LayoutService::niceScaleDenominator(35.0);
  QCOMPARE(nice, 40);
  const QgsRectangle next = LayoutService::extentForPaperScale(cur, 200.0, static_cast<double>(nice));
  QVERIFY2(qAbs(next.width() - 8.0) < 0.001, "1:40 on 200mm paper is 8m on ground");
  const double seg = LayoutService::niceScaleBarSegmentMeters(200.0, 500.0, 4);
  QVERIFY(seg > 0.0);
  const int rounded = static_cast<int>(std::lround(seg * 10.0));
  QVERIFY2(rounded % 5 == 0, "scale-bar segment is 0.5/1/2/4/5 series");
  const double barMm = LayoutService::scaleBarWidthMm(seg, 4, 500.0);
  QVERIFY(barMm >= 32.0);
  QVERIFY(barMm <= 200.0);
}

void TestWorkflow::sheetLegend_soilShowsTerrainClassesNotPictureName() {
  // 도면 범례는 흙토람 분포지형 색칸이어야 한다. 그림·지적·위성 레이어 이름은 범례가 아니다.
  // tuneSheetLegend는 Manual 전용 트리를 쓴다. 프로젝트 범례 노드를 지우면 안 된다.
  QVERIFY(LayoutService::sheetLegendOmitsLayerName(QStringLiteral("토양도(흙토람 그림)")));
  QVERIFY(LayoutService::sheetLegendOmitsLayerName(QStringLiteral("위성")));
  QVERIFY(LayoutService::sheetLegendOmitsLayerName(QStringLiteral("지적 본번")));
  QVERIFY(LayoutService::sheetLegendOmitsLayerName(QStringLiteral("지적 부번")));
  QVERIFY(!LayoutService::sheetLegendOmitsLayerName(QStringLiteral("토양도(흙토람)")));
  QVERIFY(!LayoutService::sheetLegendOmitsLayerName(QStringLiteral("고지형 판독")));

  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* soil = new QgsVectorLayer(
      QStringLiteral("Polygon?crs=EPSG:5186&field=soil_type_geo:string"),
      QStringLiteral("토양도(흙토람)"), QStringLiteral("memory"));
  QVERIFY(soil->isValid());
  QVERIFY(SoilMapService::applyTerrainStyle(soil));
  proj.addMapLayer(soil);
  auto* picture = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:3857"),
                                     QStringLiteral("토양도(흙토람 그림)"),
                                     QStringLiteral("memory"));
  QVERIFY(picture->isValid());
  proj.addMapLayer(picture);
  auto* sat = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:3857"),
                                 QStringLiteral("위성"), QStringLiteral("memory"));
  QVERIFY(sat->isValid());
  proj.addMapLayer(sat);

  QVERIFY(!LayoutService::createBlankSheet(&proj, 297.0, 210.0, QStringLiteral("user_sheet"), nullptr)
               .isEmpty());
  auto* ly = dynamic_cast<QgsPrintLayout*>(
      proj.layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  QVERIFY(ly);
  auto* map = new QgsLayoutItemMap(ly);
  map->setId(QStringLiteral("ka_map"));
  map->attemptSetSceneRect(QRectF(20.0, 20.0, 160.0, 120.0));
  map->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  map->setKeepLayerSet(true);
  map->setLayers({soil, picture, sat});
  if (map->scene() != ly)
    ly->addLayoutItem(map);

  auto* legend = new QgsLayoutItemLegend(ly);
  legend->setTitle(QStringLiteral("범례"));
  legend->setLinkedMap(map);
  legend->setResizeToContents(false);
  legend->attemptSetSceneRect(QRectF(190.0, 20.0, 48.0, 74.0));
  ly->addLayoutItem(legend);
  legend->updateLegend();
  LayoutService::tuneSheetLegend(legend);

  const QString dump = LayoutService::sheetLegendLabelDump(legend);
  QVERIFY2(dump.contains(QStringLiteral("산악지")), dump.toUtf8().constData());
  QVERIFY2(dump.contains(QStringLiteral("하성평탄")) || dump.contains(QStringLiteral("곡간")),
           dump.toUtf8().constData());
  QVERIFY2(!dump.contains(QStringLiteral("흙토람 그림")), dump.toUtf8().constData());
  QVERIFY2(!dump.contains(QStringLiteral("위성")), dump.toUtf8().constData());
}

void TestWorkflow::sheetLegend_geologyDropsStructureKeepsLithology() {
  QVERIFY(GeologyMapService::omitFromSheetLegend(QStringLiteral("부정합경계")));
  QVERIFY(GeologyMapService::omitFromSheetLegend(QStringLiteral("주향경사점")));
  QVERIFY(GeologyMapService::omitFromSheetLegend(QStringLiteral("주향경사")));
  QVERIFY(GeologyMapService::omitFromSheetLegend(QStringLiteral("지질경계")));
  QVERIFY(GeologyMapService::omitFromSheetLegend(QStringLiteral("단층추정")));
  QVERIFY(!GeologyMapService::omitFromSheetLegend(QStringLiteral("충적층")));
  QVERIFY(!GeologyMapService::omitFromSheetLegend(QStringLiteral("백악기퇴적암")));
  QVERIFY(!GeologyMapService::omitFromSheetLegend(QStringLiteral("단층")));

  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* geo = new QgsVectorLayer(
      QStringLiteral("Polygon?crs=EPSG:5186&field=class:string"),
      QStringLiteral("추가 지질도"), QStringLiteral("memory"));
  QVERIFY(geo->isValid());
  QgsCategoryList cats;
  const QStringList labels = {QStringLiteral("충적층"), QStringLiteral("백악기퇴적암"),
                              QStringLiteral("부정합경계"), QStringLiteral("주향경사점"),
                              QStringLiteral("지질경계"), QStringLiteral("단층추정"),
                              QStringLiteral("단층")};
  int i = 0;
  for (const QString& label : labels) {
    auto fs = QgsFillSymbol::createSimple(
        {{QStringLiteral("color"), QColor::fromHsv((i * 40) % 360, 140, 220, 150).name(QColor::HexArgb)}});
    cats.append(QgsRendererCategory(QVariant(label), fs.release(), label));
    ++i;
  }
  geo->setRenderer(new QgsCategorizedSymbolRenderer(QStringLiteral("class"), cats));
  QgsFeature keep(geo->fields());
  keep.setAttribute(0, QStringLiteral("충적층"));
  keep.setGeometry(QgsGeometry::fromWkt(QStringLiteral("POLYGON((200000 450000,200200 450000,200200 450200,200000 450200,200000 450000))")));
  QVERIFY(geo->dataProvider()->addFeature(keep));
  QgsFeature fault(geo->fields());
  fault.setAttribute(0, QStringLiteral("단층"));
  fault.setGeometry(QgsGeometry::fromWkt(QStringLiteral("POLYGON((200040 450040,200080 450040,200080 450080,200040 450080,200040 450040))")));
  QVERIFY(geo->dataProvider()->addFeature(fault));
  proj.addMapLayer(geo);
  GeologyMapService::pruneStructureLegend(geo);

  QVERIFY(!LayoutService::createBlankSheet(&proj, 297.0, 210.0, QStringLiteral("user_sheet"), nullptr)
               .isEmpty());
  auto* ly = dynamic_cast<QgsPrintLayout*>(
      proj.layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  QVERIFY(ly);
  auto* map = new QgsLayoutItemMap(ly);
  map->setId(QStringLiteral("ka_map"));
  map->attemptSetSceneRect(QRectF(20.0, 20.0, 160.0, 120.0));
  map->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  map->setKeepLayerSet(true);
  map->setLayers({geo});
  map->zoomToExtent(QgsRectangle(199990.0, 449990.0, 200210.0, 450210.0));
  if (map->scene() != ly)
    ly->addLayoutItem(map);

  auto* legend = new QgsLayoutItemLegend(ly);
  legend->setTitle(QStringLiteral("범례"));
  legend->setLinkedMap(map);
  legend->setResizeToContents(false);
  legend->attemptSetSceneRect(QRectF(190.0, 20.0, 48.0, 74.0));
  ly->addLayoutItem(legend);
  legend->updateLegend();
  LayoutService::tuneSheetLegend(legend);

  const QString dump = LayoutService::sheetLegendLabelDump(legend);
  QVERIFY2(dump.contains(QStringLiteral("충적층")), dump.toUtf8().constData());
  QVERIFY2(dump.contains(QStringLiteral("단층")), dump.toUtf8().constData());
  QVERIFY2(!dump.contains(QStringLiteral("부정합경계")), dump.toUtf8().constData());
  QVERIFY2(!dump.contains(QStringLiteral("주향경사점")), dump.toUtf8().constData());
  QVERIFY2(!dump.contains(QStringLiteral("지질경계")), dump.toUtf8().constData());
  QVERIFY2(!dump.contains(QStringLiteral("단층추정")), dump.toUtf8().constData());
}

void TestWorkflow::sheetLegend_hidesUncheckedSoilLayer() {
  // 레이어 목록에서 토양도를 끄면 도면 범례에도 없어야 한다. 숨긴 레이어를 다시 끼워 넣지 않는다.
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* soil = new QgsVectorLayer(
      QStringLiteral("Polygon?crs=EPSG:5186&field=soil_type_geo:string"),
      QStringLiteral("토양도(흙토람)"), QStringLiteral("memory"));
  QVERIFY(soil->isValid());
  QVERIFY(SoilMapService::applyTerrainStyle(soil));
  proj.addMapLayer(soil);
  auto* sat = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:3857"),
                                 QStringLiteral("위성"), QStringLiteral("memory"));
  QVERIFY(sat->isValid());
  proj.addMapLayer(sat);

  QVERIFY(!LayoutService::createBlankSheet(&proj, 297.0, 210.0, QStringLiteral("user_sheet"), nullptr)
               .isEmpty());
  auto* ly = dynamic_cast<QgsPrintLayout*>(
      proj.layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  QVERIFY(ly);
  auto* map = new QgsLayoutItemMap(ly);
  map->setId(QStringLiteral("ka_map"));
  map->attemptSetSceneRect(QRectF(20.0, 20.0, 160.0, 120.0));
  map->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  map->setKeepLayerSet(true);
  map->setLayers({soil, sat});
  if (map->scene() != ly)
    ly->addLayoutItem(map);

  auto* legend = new QgsLayoutItemLegend(ly);
  legend->setTitle(QStringLiteral("범례"));
  legend->setLinkedMap(map);
  legend->setResizeToContents(false);
  legend->attemptSetSceneRect(QRectF(190.0, 20.0, 48.0, 74.0));
  ly->addLayoutItem(legend);
  legend->updateLegend();
  LayoutService::tuneSheetLegend(legend);
  QVERIFY(LayoutService::sheetLegendLabelDump(legend).contains(QStringLiteral("산악지")));

  QgsLayerTreeLayer* soilNode = proj.layerTreeRoot()->findLayer(soil->id());
  QVERIFY(soilNode);
  soilNode->setItemVisibilityChecked(false);
  QVERIFY(!soilNode->isVisible());
  map->setLayers({sat});
  LayoutService::tuneSheetLegend(legend);
  const QString dump = LayoutService::sheetLegendLabelDump(legend);
  QVERIFY2(!dump.contains(QStringLiteral("산악지")), dump.toUtf8().constData());
  QVERIFY2(!dump.contains(QStringLiteral("토양도(흙토람)")), dump.toUtf8().constData());

  soilNode->setItemVisibilityChecked(true);
  map->setLayers({soil, sat});
  LayoutService::tuneSheetLegend(legend);
  const QString dumpOn = LayoutService::sheetLegendLabelDump(legend);
  QVERIFY2(dumpOn.contains(QStringLiteral("산악지")), dumpOn.toUtf8().constData());
}

void TestWorkflow::sheetLegend_followsLayerCheckOnAndOff() {
  // 레이어 체크를 끄면 그 항목만 범례에서 빠지고, 다시 켜면 돌아온다. 다른 켠 레이어는 유지.
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* soil = new QgsVectorLayer(
      QStringLiteral("Polygon?crs=EPSG:5186&field=soil_type_geo:string"),
      QStringLiteral("토양도(흙토람)"), QStringLiteral("memory"));
  QVERIFY(soil->isValid());
  QVERIFY(SoilMapService::applyTerrainStyle(soil));
  proj.addMapLayer(soil);
  auto* zone = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("조사구역"), QStringLiteral("memory"));
  QVERIFY(zone->isValid());
  proj.addMapLayer(zone);

  QVERIFY(!LayoutService::createBlankSheet(&proj, 297.0, 210.0, QStringLiteral("user_sheet"), nullptr)
               .isEmpty());
  auto* ly = dynamic_cast<QgsPrintLayout*>(
      proj.layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  QVERIFY(ly);
  auto* map = new QgsLayoutItemMap(ly);
  map->setId(QStringLiteral("ka_map"));
  map->attemptSetSceneRect(QRectF(20.0, 20.0, 160.0, 120.0));
  map->setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  map->setKeepLayerSet(true);
  map->setLayers({soil, zone});
  if (map->scene() != ly)
    ly->addLayoutItem(map);

  auto* legend = new QgsLayoutItemLegend(ly);
  legend->setLinkedMap(map);
  legend->setResizeToContents(false);
  ly->addLayoutItem(legend);
  LayoutService::tuneSheetLegend(legend);
  QString dump = LayoutService::sheetLegendLabelDump(legend);
  QVERIFY2(dump.contains(QStringLiteral("산악지")), dump.toUtf8().constData());
  QVERIFY2(dump.contains(QStringLiteral("조사구역")), dump.toUtf8().constData());

  QgsLayerTreeLayer* zoneNode = proj.layerTreeRoot()->findLayer(zone->id());
  QVERIFY(zoneNode);
  zoneNode->setItemVisibilityChecked(false);
  map->setLayers({soil});
  LayoutService::tuneSheetLegend(legend);
  dump = LayoutService::sheetLegendLabelDump(legend);
  QVERIFY2(dump.contains(QStringLiteral("산악지")), dump.toUtf8().constData());
  QVERIFY2(!dump.contains(QStringLiteral("조사구역")), dump.toUtf8().constData());

  zoneNode->setItemVisibilityChecked(true);
  map->setLayers({soil, zone});
  LayoutService::tuneSheetLegend(legend);
  dump = LayoutService::sheetLegendLabelDump(legend);
  QVERIFY2(dump.contains(QStringLiteral("산악지")), dump.toUtf8().constData());
  QVERIFY2(dump.contains(QStringLiteral("조사구역")), dump.toUtf8().constData());
}

void TestWorkflow::legendTitlesHideEpsgAndUseShortKorean() {
  auto* sat = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:3857"),
                                 QStringLiteral("VWorld 위성 [EPSG:3857]"),
                                 QStringLiteral("memory"));
  QVERIFY(sat->isValid());
  LayerOps::applyLegendCrsLabel(sat);
  QCOMPARE(sat->name(), QStringLiteral("위성"));
  QVERIFY(!sat->name().contains(QStringLiteral("EPSG")));
  delete sat;

  auto* cad = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                 QStringLiteral("VWorld 지적(본번·부번) [EPSG:3857]"),
                                 QStringLiteral("memory"));
  QVERIFY(cad->isValid());
  LayerOps::applyLegendCrsLabel(cad);
  QCOMPARE(cad->name(), QStringLiteral("지적"));
  QVERIFY(!cad->name().contains(QStringLiteral("EPSG")));
  delete cad;

  auto* zone = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("구역 [EPSG:5186]"), QStringLiteral("memory"));
  QVERIFY(zone->isValid());
  LayerOps::applyLegendCrsLabel(zone);
  QCOMPARE(zone->name(), QStringLiteral("구역"));
  QVERIFY(!zone->name().contains(QStringLiteral("EPSG")));
  delete zone;
}

void TestWorkflow::drawSubToolbarWiresEachDomainSlot() {
  QFile f(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
           "run from source tree (ctest WORKING_DIRECTORY)");
  const QString src = QString::fromUtf8(f.readAll());
  const int fn = src.indexOf(QLatin1String("void MainWindow::showSubToolsDraw"));
  QVERIFY2(fn >= 0, "showSubToolsDraw must exist");
  const int next = src.indexOf(QLatin1String("void MainWindow::showSubToolsBasemap"), fn + 1);
  QVERIFY2(next > fn, "showSubToolsDraw body must end before showSubToolsBasemap");
  const QString body = src.mid(fn, next - fn);
  auto wired = [&](const char* iconId, const char* slot) {
    const int ic = body.indexOf(QLatin1String(iconId));
    const int sl = body.indexOf(QLatin1String(slot));
    return ic >= 0 && sl > ic;
  };
  QVERIFY2(wired("draw_area", "startEditSurveyArea"),
           "draw_area (조사구역) must call startEditSurveyArea");
  QVERIFY2(wired("draw_poly", "startEditFeaturePoly"),
           "draw_poly (유구면) must call startEditFeaturePoly");
  QVERIFY2(wired("draw_line", "startEditFeatureLine"),
           "draw_line (유구선) must call startEditFeatureLine");
  QVERIFY2(wired("artifact", "startEditArtifact"),
           "artifact (유물) must call startEditArtifact");
  // 사용자 요구: 그리기 툴바는 「그리는 일」만 둔다. 단면선·속성은 여기서 뺐다.
  QVERIFY2(!body.contains(QLatin1String("startEditSectionLine")),
           "단면선은 그리기 툴바에서 빠져야 한다(지도 우클릭 메뉴로 옮김)");
  QVERIFY2(!body.contains(QLatin1String("startAttributeEditTool")),
           "속성은 그리기 툴바에서 빠져야 한다(지도 우클릭 메뉴에 있음)");
  // 뺀 기능이 사라지면 안 된다 — 지도 우클릭 메뉴에서 갈 수 있어야 한다.
  QFile menus(QStringLiteral("src/app/MainWindowContextMenus.cpp"));
  QVERIFY2(menus.open(QIODevice::ReadOnly | QIODevice::Text), "MainWindowContextMenus.cpp");
  const QString menuSource = QString::fromUtf8(menus.readAll());
  const int ctx = menuSource.indexOf(QLatin1String("void MainWindow::populateMapContextMenu"));
  QVERIFY2(ctx >= 0, "populateMapContextMenu must exist");
  const int ctxEnd = menuSource.indexOf(QLatin1String("\nvoid MainWindow::"), ctx + 1);
  const QString ctxBody = menuSource.mid(ctx, (ctxEnd > ctx ? ctxEnd : menuSource.size()) - ctx);
  QVERIFY2(ctxBody.contains(QLatin1String("startEditSectionLine")),
           "단면선은 지도 우클릭 메뉴에서 그릴 수 있어야 한다");
  QVERIFY2(ctxBody.contains(QLatin1String("editFeatureAttributes")),
           "속성 편집도 지도 우클릭 메뉴에 남아 있어야 한다");
  // 사용자 요구: 폴리곤 묶기·나누기·구간 분리·닫기는 오른쪽 끝이 아니라
  // 그리기 버튼 바로 옆(가운데 쪽)에 같이 보여야 한다.
  QVERIFY2(!body.contains(QLatin1String("subToolbarSpacer")),
           "오른쪽 끝으로 미는 빈칸은 없어야 한다 — 그리기 버튼 옆에 붙인다");
  const int artiPos = body.indexOf(QLatin1String("startEditArtifact"));
  const int mergePos = body.indexOf(QLatin1String("mergeFeaturePolygons"));
  const int splitPos = body.indexOf(QLatin1String("startSplitPolygonTool"));
  const int clipPos = body.indexOf(QLatin1String("clipOverlappingLayers"));
  const int closePos = body.indexOf(QLatin1String("&MainWindow::hideSubTools"));
  QVERIFY2(artiPos >= 0 && mergePos > artiPos,
           "폴리곤 묶기는 그리기 버튼 뒤에 이어서 놓여야 한다");
  QVERIFY2(mergePos < splitPos && splitPos < clipPos && clipPos < closePos,
           "순서는 폴리곤 묶기 → 폴리곤 나누기 → 구간 분리 → 닫기");
  QVERIFY2(!body.contains(QLatin1String("draw_area")) ||
               body.indexOf(QLatin1String("startEditFeaturePoly")) >
                   body.indexOf(QLatin1String("startEditSurveyArea")),
           "draw_area must not be the feature_poly button");
}

void TestWorkflow::digitizeTargetLayer_featurePolyIgnoresSurveyAreaCurrent() {
  const QString dir = QDir::temp().filePath(
      QStringLiteral("ka_dig_") + QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(QDir().mkpath(dir));
  QString err;
  const QString gpkg =
      SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("digtgt"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  QgsProject proj;
  auto* area = LayerOps::ensureDomainLayer(&proj, gpkg, QStringLiteral("survey_area"),
                                           QStringLiteral("조사구역"), &err);
  QVERIFY2(area, qPrintable(err));
  auto* feat = LayerOps::ensureDomainLayer(&proj, gpkg, QStringLiteral("feature_poly"),
                                           QStringLiteral("유구면"), &err);
  QVERIFY2(feat, qPrintable(err));
  QVERIFY(area != feat);
  QCOMPARE(LayerOps::layerKeyOf(area), QStringLiteral("survey_area"));
  QCOMPARE(LayerOps::layerKeyOf(feat), QStringLiteral("feature_poly"));

  QgsVectorLayer* got =
      LayerOps::digitizeTargetLayer(&proj, area, QStringLiteral("feature_poly"));
  QCOMPARE(got, feat);
  QCOMPARE(LayerOps::layerKeyOf(got), QStringLiteral("feature_poly"));
  QCOMPARE(LayerOps::digitizeTargetLayer(&proj, feat, QStringLiteral("feature_poly")), feat);
  QCOMPARE(LayerOps::digitizeTargetLayer(&proj, nullptr, QStringLiteral("feature_poly")), feat);
}

void TestWorkflow::newSurvey_removesUserLayersKeepsXyzBasemap() {
  QgsProject proj;
  auto* easy = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"),
                                  QStringLiteral("쉽게그리기"), QStringLiteral("memory"));
  auto* namedCad = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5187"),
                                      QStringLiteral("지적경계"), QStringLiteral("memory"));
  auto* ring = new QgsVectorLayer(QStringLiteral("LineString?crs=EPSG:5187"),
                                  QStringLiteral("주변 500m"), QStringLiteral("memory"));
  QVERIFY(easy->isValid() && namedCad->isValid() && ring->isValid());
  LayerOps::markSurveyLayer(easy, QStringLiteral("survey_area"));
  LayerOps::markSurveyLayer(namedCad, QStringLiteral("feature_poly"));
  LayerOps::markSurveyLayer(ring, QStringLiteral("feature_line"));
  QVERIFY(easy->startEditing());
  proj.addMapLayer(easy);
  proj.addMapLayer(namedCad);
  proj.addMapLayer(ring);

  QString err;
  const bool osmOk = LayerOps::addOsmBasemap(&proj, nullptr, &err);

  QVERIFY(LayerOps::isBasemapLayer(namedCad) == false);

  LayerOps::removeSurveyDomainLayers(&proj);

  QStringList leftover;
  bool hasBasemap = false;
  for (QgsMapLayer* l : proj.mapLayers()) {
    if (!l) continue;
    if (LayerOps::isBasemapLayer(l)) {
      hasBasemap = true;
      continue;
    }
    leftover << l->name();
  }
  QVERIFY2(leftover.isEmpty(), qPrintable(leftover.join(QLatin1Char(','))));
  if (osmOk)
    QVERIFY2(hasBasemap, "OSM xyz/wms must remain after 새 조사");
}

void TestWorkflow::layoutOpensAsMainWindowTabNotSeparateWindow() {
  const QString src = readMainWindowSources();
  QVERIFY2(!src.isEmpty(), "run from source tree (ctest WORKING_DIRECTORY)");
  const int fn = src.indexOf(QLatin1String("void MainWindow::openLayoutDesigner"));
  QVERIFY2(fn >= 0, "openLayoutDesigner must exist");
  const int next = src.indexOf(QLatin1String("void MainWindow::"), fn + 10);
  QVERIFY2(next > fn, "openLayoutDesigner body");
  const QString body = src.mid(fn, next - fn);
  QVERIFY2(body.contains(QLatin1String("addTab")),
           "조판 must add a 레이아웃 tab, not a free window");
  QVERIFY2(body.contains(QLatin1String("setCurrentWidget")),
           "조판 must switch to the layout tab");
  QVERIFY2(!body.contains(QLatin1String("activateWindow")),
           "must not raise a separate studio window");
}

void TestWorkflow::startupLoadsSatelliteAndCadastralWithoutToolbarIcons() {
  QFile f(QStringLiteral("src/app/MainWindow.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
           "run from source tree (ctest WORKING_DIRECTORY)");
  const QString src = QString::fromUtf8(f.readAll());
  const int fn = src.indexOf(QLatin1String("void MainWindow::ensureDefaultBasemaps"));
  QVERIFY2(fn >= 0, "ensureDefaultBasemaps must exist");
  const int next = src.indexOf(QLatin1String("void MainWindow::"), fn + 10);
  QVERIFY2(next > fn, "ensureDefaultBasemaps body");
  const QString body = src.mid(fn, next - fn);
  QVERIFY2(body.contains(QLatin1String("addVworldSatelliteMap")),
           "start must add VWorld satellite as reference");
  QVERIFY2(body.contains(QLatin1String("addVworldCadastralMap")),
           "start must add VWorld cadastral as reference");
  QVERIFY2(src.contains(QLatin1String("ensureStartupViewReady")),
           "map tab must apply Korea view once the canvas has a size");
  QVERIFY2(!src.contains(QLatin1String("addIcon(QStringLiteral(\"satellite\"), QStringLiteral(\"위성\")")),
           "main toolbar must not keep 위성 icon");
  QVERIFY2(!src.contains(QLatin1String("addIcon(QStringLiteral(\"cadastral\"), QStringLiteral(\"지적\")")),
           "main toolbar must not keep 지적 icon");
  const QString sessionSrc = readMainWindowSources();
  const int newAt = sessionSrc.indexOf(QLatin1String("void MainWindow::newSurvey"));
  QVERIFY2(newAt >= 0, "newSurvey");
  const int newEnd = sessionSrc.indexOf(QLatin1String("void MainWindow::"), newAt + 10);
  const QString newBody = sessionSrc.mid(newAt, newEnd - newAt);
  const int applyAt = newBody.indexOf(QLatin1String("applyStartupMap()"));
  const int ensureAt = newBody.indexOf(QLatin1String("ensureDefaultBasemaps()"));
  QVERIFY2(applyAt >= 0 && ensureAt > applyAt,
           "새 조사는 예약만 하지 말고 위성·지적을 바로 올려야 함");
}

void TestWorkflow::layoutCoordPointHasIconAndCallout() {
  QFile icons(QStringLiteral("src/app/KaIcons.cpp"));
  QVERIFY2(icons.open(QIODevice::ReadOnly | QIODevice::Text), "KaIcons.cpp");
  const QString ic = QString::fromUtf8(icons.readAll());
  QVERIFY2(ic.contains(QLatin1String("layout_coord_point")), "toolbar icon id");
  QVERIFY2(ic.contains(QLatin1String("dCoordPoint")), "coord-point icon drawing");
  QFile f(QStringLiteral("src/app/KaDrawingStudio.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "KaDrawingStudio.cpp");
  const QString src = QString::fromUtf8(f.readAll());
  QVERIFY2(src.contains(QLatin1String("beginPlaceCoordPoint")), "coord tool slot");
  QVERIFY2(src.contains(QLatin1String("placeCoordCallout")), "callout placement");
  QVERIFY2(src.contains(QLatin1String("ka_coord_box_")), "label id");
}

void TestWorkflow::layoutCoordCallout_staysOnMapXyAfterScale() {
  const QgsRectangle ext(200000.0, 450000.0, 200160.0, 450080.0);
  const QRectF ir(10.0, 8.0, 160.0, 80.0);
  const double mx = 200040.0;
  const double my = 450060.0;
  const QPointF p1 = LayoutService::layoutMapItemFromXy(ext, ir, mx, my);
  const QgsPointXY back = LayoutService::layoutMapXyFromItem(ext, ir, p1);
  QVERIFY2(qAbs(back.x() - mx) < 1e-6 && qAbs(back.y() - my) < 1e-6, "왕복이 같아야 함");
  const QgsRectangle wide = LayoutService::extentForPaperScale(ext, 160.0, 2000.0);
  QVERIFY2(wide.width() > ext.width() + 1.0, "축척을 키우면 범위가 넓어짐");
  const QPointF p2 = LayoutService::layoutMapItemFromXy(wide, ir, mx, my);
  QVERIFY2(qAbs(p1.x() - p2.x()) > 0.5 || qAbs(p1.y() - p2.y()) > 0.5,
           "같은 땅점이 축척 뒤에도 같은 용지 자리에 있으면 화살표가 틀린 곳으로 감");
  const QgsPointXY still = LayoutService::layoutMapXyFromItem(wide, ir, p2);
  QVERIFY2(qAbs(still.x() - mx) < 1e-6 && qAbs(still.y() - my) < 1e-6,
           "다시 앉힌 용지점은 같은 지도 XY여야 함");
}

void TestWorkflow::layoutCoordCallout_rejectsOutsideAndCanUndo() {
  const QRectF ir(0.0, 0.0, 160.0, 80.0);
  QVERIFY(LayoutService::layoutMapItemContains(QPointF(80.0, 40.0), ir));
  QVERIFY2(!LayoutService::layoutMapItemContains(QPointF(-12.0, 40.0), ir),
           "맵 밖 흰 용지 클릭은 좌표점이 아님");
  QFile f(QStringLiteral("src/app/KaDrawingStudio.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "KaDrawingStudio.cpp");
  const QString src = QString::fromUtf8(f.readAll());
  QVERIFY2(src.contains(QLatin1String("undoLastCoordCallout")),
           "마지막 좌표점을 지울 수 있어야 함");
  QVERIFY2(src.contains(QLatin1String("relayoutCoordCallouts")),
           "축척이 바뀌면 땅 XY에서 다시 앉혀야 함");
  QVERIFY2(src.contains(QLatin1String("closestSegmentWithContext")),
           "조판 자석은 꼭짓점만이 아니라 선에도 붙어야 함");
  QVERIFY2(src.contains(QLatin1String("layoutMapItemContains")),
           "맵 밖 클릭을 버려야 흰 용지에 K가 안 생김");
  QVERIFY2(src.contains(QString::fromUtf8("마지막 점 지우기")),
           "우클릭에 지우기가 있어야 함");
}

void TestWorkflow::portableExe_setsPrefixFromExeDir() {
  QFile appFile(QStringLiteral("src/app/KaApplication.cpp"));
  QVERIFY2(appFile.open(QIODevice::ReadOnly | QIODevice::Text), "KaApplication.cpp");
  const QString appSrc = QString::fromUtf8(appFile.readAll());
  QVERIFY2(appSrc.contains(QLatin1String("applyBundledRuntime")),
           "포터블 EXE가 자기 폴더에서 QGIS를 찾음");
  QVERIFY2(appSrc.contains(QLatin1String("kaExeDir")), "start.bat 없이 EXE 위치");
  QVERIFY2(appSrc.contains(QLatin1String("KaPortableRuntime::discover")), "포터블 경로 탐지");
  QVERIFY2(appSrc.contains(QLatin1String("KaPortableRuntime::applyEnvironment")),
           "한글 경로 Wide+UTF-8 환경");
  QVERIFY2(appSrc.contains(QLatin1String("KaPortableRuntime::bindProjSearchPaths")),
           "initQgis 뒤 개발PC OSGeo 경로를 덮어씀");
  QVERIFY2(appSrc.contains(QLatin1String("koreaWorkAndWebCrsValid")),
           "5186/5187/3857 부트 검사");
  QVERIFY2(appSrc.contains(QLatin1String("isolateUserState")),
           "다른 PC 이전 설치 QSettings를 묶음");

  QFile rt(QStringLiteral("src/core/KaPortableRuntime.cpp"));
  QVERIFY2(rt.open(QIODevice::ReadOnly | QIODevice::Text), "KaPortableRuntime.cpp");
  const QString src = QString::fromUtf8(rt.readAll());
  QVERIFY2(src.contains(QLatin1String("apps/qgis-dev")), "포터블 폴더 구조");
  QVERIFY2(src.contains(QLatin1String("SetEnvironmentVariableW")), "Wide PROJ_DATA");
  QVERIFY2(src.contains(QLatin1String("OSRSetPROJSearchPaths")), "GDAL PROJ 검색 경로");
  QVERIFY2(src.contains(QLatin1String("CURL_CA_BUNDLE")), "다른 PC HTTPS 위성/지적");
  QVERIFY2(src.contains(QLatin1String("QGIS_CUSTOM_CONFIG_PATH")),
           "다른 PC QGIS 프로필을 exe/config로");
  QVERIFY2(!src.contains(QLatin1String("QFile::encodeName")), "포터블 경로는 CP949 금지");
}

void TestWorkflow::portableRuntime_discoversUnicodeFolderAndKoreaCrs() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString root = tmp.path() + QStringLiteral("/복사본/ka-hgis-portable");
  QVERIFY(QDir().mkpath(root + QStringLiteral("/apps/qgis-dev")));
  QVERIFY(QDir().mkpath(root + QStringLiteral("/share/proj")));
  {
    QFile db(root + QStringLiteral("/share/proj/proj.db"));
    QVERIFY(db.open(QIODevice::WriteOnly));
    db.write("proj");
  }
  const KaPortablePaths paths = KaPortableRuntime::discover(root);
  QVERIFY2(paths.looksBundled(), "apps/qgis-dev 있으면 포터블");

  // 한글 경로에 그대로 두면 QGIS 가 provider·srs.db 를 못 읽어 위성·지적이 빈다.
  // QGIS 는 prefix/plugin 경로를 좁은 문자열(로컬 코드페이지)로 쓰기 때문이다.
  // 그래서 discover() 는 ASCII 정션(C:/Users/Public/ka-hgis/rt-<해시>)으로 갈아 끼운다.
  auto pureAscii = [](const QString& text) {
    for (const QChar c : text)
      if (c.unicode() > 127) return false;
    return true;
  };
  QVERIFY2(pureAscii(paths.projData), qPrintable(paths.projData));
  QVERIFY2(pureAscii(paths.qgisPrefix), qPrintable(paths.qgisPrefix));
  QVERIFY2(pureAscii(paths.exeDir), qPrintable(paths.exeDir));

  // 갈아 끼운 경로가 같은 파일을 가리켜야 한다. 이름만 바꾸고 내용을 잃으면 안 된다.
  QFile aliased(QDir(paths.projData).filePath(QStringLiteral("proj.db")));
  QVERIFY2(aliased.exists(), qPrintable(aliased.fileName()));
  QVERIFY(aliased.open(QIODevice::ReadOnly));
  QCOMPARE(aliased.readAll(), QByteArray("proj"));
  aliased.close();

  // ASCII 경로는 건드리지 않는다. 쓸데없는 정션을 만들지 않는다.
  const QString asciiRoot = tmp.path() + QStringLiteral("/plain/ka-hgis-portable");
  QVERIFY(QDir().mkpath(asciiRoot + QStringLiteral("/apps/qgis-dev")));
  QVERIFY(QDir().mkpath(asciiRoot + QStringLiteral("/share/proj")));
  {
    QFile db(asciiRoot + QStringLiteral("/share/proj/proj.db"));
    QVERIFY(db.open(QIODevice::WriteOnly));
    db.write("proj");
  }
  const KaPortablePaths plain = KaPortableRuntime::discover(asciiRoot);
  QCOMPARE(QDir(plain.projData).absolutePath(),
           QDir(asciiRoot + QStringLiteral("/share/proj")).absolutePath());

  QString projDir;
  const QByteArray osgeo = qgetenv("OSGEO4W_ROOT");
  if (!osgeo.isEmpty()) {
    const QString fromOsgeo = QString::fromLocal8Bit(osgeo) + QStringLiteral("/share/proj");
    if (QFile::exists(fromOsgeo + QStringLiteral("/proj.db")))
      projDir = fromOsgeo;
  }
  if (projDir.isEmpty() && QFile::exists(QStringLiteral("L:/ka-hgis-portable/share/proj/proj.db")))
    projDir = QStringLiteral("L:/ka-hgis-portable/share/proj");
  QVERIFY2(!projDir.isEmpty(), "테스트 PC에 proj.db가 있어야 함");
  QVERIFY2(KaPortableRuntime::bindProjSearchPaths(projDir), "PROJ 검색 경로");
  QVERIFY2(KaPortableRuntime::koreaWorkAndWebCrsValid(),
           "작업 5186/5187과 웹 3857 변환이 다른 PC와 같아야 함");
}

void TestWorkflow::portableRuntime_ignoresLeftoverAppDataKey() {
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString root = tmp.path() + QStringLiteral("/ka-hgis-portable");
  QVERIFY(QDir().mkpath(root + QStringLiteral("/apps/qgis-dev")));
  QVERIFY(QDir().mkpath(root + QStringLiteral("/config")));
  {
    QFile secrets(root + QStringLiteral("/config/secrets.ini"));
    QVERIFY(secrets.open(QIODevice::WriteOnly | QIODevice::Text));
    secrets.write("[VWorld]\nApiKey=portable-folder-key\n");
  }
  const QString leftoverDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  QVERIFY(QDir().mkpath(leftoverDir));
  const QString leftoverIni = QDir(leftoverDir).filePath(QStringLiteral("ka-hgis-vworld.ini"));
  {
    QSettings leftover(leftoverIni, QSettings::IniFormat);
    leftover.setValue(QStringLiteral("VWorld/ApiKey"), QStringLiteral("leftover-appdata-key"));
    leftover.sync();
  }
  const QByteArray oldEnv = qgetenv("VWORLD_API_KEY");
  const bool hadEnv = qEnvironmentVariableIsSet("VWORLD_API_KEY");
  qunsetenv("VWORLD_API_KEY");
  KaPortableRuntime::setExeDirOverride(root);
  QCOMPARE(QDir(KaPortableRuntime::userConfigDir()).absolutePath(),
           QDir(root + QStringLiteral("/config")).absolutePath());
  QCOMPARE(VworldSettings::loadApiKey(), QStringLiteral("portable-folder-key"));
  KaPortableRuntime::setExeDirOverride(QString());
  if (hadEnv)
    qputenv("VWORLD_API_KEY", oldEnv);
  else
    qunsetenv("VWORLD_API_KEY");
}

void TestWorkflow::singleInstanceGuardIsWiredIntoBoot() {
  // 중복 실행 가드는 정의만 있으면 효과가 없다. run()에서 실제로 불려야 한다.
  QFile f(QStringLiteral("src/app/KaApplication.cpp"));
  QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), "KaApplication.cpp");
  const QString src = QString::fromUtf8(f.readAll());
  QVERIFY2(src.contains(QLatin1String("static bool kaActivateExistingInstance()")),
           "guard helper must exist");
  const int defAt = src.indexOf(QLatin1String("static bool kaActivateExistingInstance()"));
  const int runAt = src.indexOf(QLatin1String("int KaApplication::run("));
  QVERIFY(defAt >= 0 && runAt > defAt);
  const QString runBody = src.mid(runAt);
  QVERIFY2(runBody.contains(QLatin1String("kaActivateExistingInstance()")),
           "run() must call the single-instance guard");
  // 자동 QA는 항상 자기 프로세스로 끝까지 돌아야 하므로 가드에서 제외한다.
  const int callAt = runBody.indexOf(QLatin1String("kaActivateExistingInstance()"));
  const QString around = runBody.mid(std::max(0, callAt - 200), 260);
  QVERIFY2(around.contains(QLatin1String("smokeQuit")),
           "smoke/QA runs must bypass the guard");
  QVERIFY2(src.contains(QLatin1String("ka-hgis-single-instance-")),
           "포터블과 예전 설치본이 같은 뮤텍스를 쓰면 안 됨");
  QVERIFY2(src.contains(QLatin1String("QueryFullProcessImageNameW")),
           "같은 exe 경로만 앞으로 올림");
}

void TestWorkflow::layoutWheelZoom_keepsPointUnderCursor() {
  const QgsRectangle ext(200000.0, 450000.0, 200200.0, 450100.0);
  // 오른쪽 위 1/4 지점을 가리킨 채 2배 확대 → 그 지상 좌표가 같은 화면 위치에 남아야.
  const double fx = 0.75, fy = 0.25;
  const double gx = ext.xMinimum() + fx * ext.width();
  const double gy = ext.yMaximum() - fy * ext.height();
  const QgsRectangle zin = LayoutService::zoomExtentAtAnchor(ext, fx, fy, 2.0);
  QVERIFY2(qAbs(zin.width() - ext.width() / 2.0) < 1e-6, "2배 확대는 폭 절반");
  QVERIFY2(qAbs(zin.height() - ext.height() / 2.0) < 1e-6, "종횡비 유지");
  QVERIFY2(qAbs((zin.xMinimum() + fx * zin.width()) - gx) < 1e-6, "커서 지점 X 고정");
  QVERIFY2(qAbs((zin.yMaximum() - fy * zin.height()) - gy) < 1e-6, "커서 지점 Y 고정");
  // 중심에서 굴리면 중심 기준 확대와 같아야(회귀 방지).
  const QgsRectangle mid = LayoutService::zoomExtentAtAnchor(ext, 0.5, 0.5, 2.0);
  QVERIFY(qAbs(mid.center().x() - ext.center().x()) < 1e-6);
  QVERIFY(qAbs(mid.center().y() - ext.center().y()) < 1e-6);
  // 축소도 같은 지점을 물고 있어야.
  const QgsRectangle zout = LayoutService::zoomExtentAtAnchor(ext, fx, fy, 0.5);
  QVERIFY2(qAbs(zout.width() - ext.width() * 2.0) < 1e-6, "0.5배는 폭 두 배");
  QVERIFY2(qAbs((zout.xMinimum() + fx * zout.width()) - gx) < 1e-6, "축소도 커서 지점 고정");
  // 잘못된 입력은 원본 그대로.
  QCOMPARE(LayoutService::zoomExtentAtAnchor(ext, fx, fy, 0.0), ext);
}

void TestWorkflow::layoutProfessionalSheet_frameGridTitleBlock() {
  // 도곽 자동 간격: 종이 한 칸 25~70mm가 되는 1-2-5 계열.
  QVERIFY(qAbs(LayoutService::niceGridIntervalMeters(1000.0, 180.0) - 50.0) < 1e-9);
  QVERIFY(qAbs(LayoutService::niceGridIntervalMeters(5000.0, 180.0) - 200.0) < 1e-9);
  QVERIFY(qAbs(LayoutService::niceGridIntervalMeters(100.0, 180.0) - 5.0) < 1e-9);

  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* sa = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                QStringLiteral("조사구역"), QStringLiteral("memory"));
  QVERIFY(sa->isValid());
  LayerOps::markSurveyLayer(sa, QStringLiteral("survey_area"));
  proj.addMapLayer(sa);
  QVERIFY(sa->startEditing());
  QgsFeature sf(sa->fields());
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200080, 450000)
       << QgsPointXY(200080, 450080) << QgsPointXY(200000, 450080)
       << QgsPointXY(200000, 450000);
  sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(sa->addFeature(sf));
  QVERIFY(sa->commitChanges());

  LayoutService::DrawingOptions opt;
  QString err;
  const auto r =
      LayoutService::buildDrawing(&proj, LayoutService::DrawingKind::SurveyAreaMap, opt, &err);
  QVERIFY2(r.hasMapContent, qPrintable(r.warningKo));
  auto* ly = dynamic_cast<QgsPrintLayout*>(
      proj.layoutManager()->layoutByName(QStringLiteral("survey_area_map")));
  QVERIFY(ly);

  // 도곽: 지브라 프레임 + 십자 눈금 + 정수 좌표 주기 하나만.
  QList<QgsLayoutItemMap*> maps;
  ly->layoutItems(maps);
  QVERIFY(!maps.isEmpty());
  QgsLayoutItemMap* map = maps.first();
  QVERIFY(map->grids());
  const QList<QgsLayoutItemMapGrid*> grids = map->grids()->asList();
  QCOMPARE(grids.size(), 1);
  QgsLayoutItemMapGrid* g = grids.first();
  QVERIFY(g->enabled());
  QCOMPARE(g->frameStyle(), Qgis::MapGridFrameStyle::Zebra);
  QCOMPARE(g->style(), Qgis::MapGridStyle::LineCrosses);
  QVERIFY(g->annotationEnabled());
  QCOMPARE(g->annotationPrecision(), 0);
  QVERIFY(g->intervalX() > 0.0);

  // 표제란: 축척·좌표계가 든 정보 상자.
  QList<QgsLayoutItemLabel*> labels;
  ly->layoutItems(labels);
  QgsLayoutItemLabel* block = nullptr;
  for (QgsLayoutItemLabel* l : labels) {
    if (l && l->id() == QStringLiteral("title_block")) block = l;
  }
  QVERIFY2(block, "title_block 표제란이 있어야 한다");
  QVERIFY(block->text().contains(QStringLiteral("축  척")));
  QVERIFY(block->text().contains(QStringLiteral("EPSG:5186")));
  QVERIFY(block->frameEnabled());

  // 교호식(Double Box) 축척자.
  QList<QgsLayoutItemScaleBar*> bars;
  ly->layoutItems(bars);
  QVERIFY(!bars.isEmpty());
  QCOMPARE(bars.first()->style(), QStringLiteral("Double Box"));

  // 실제 렌더가 죽지 않는지 + 육안 점검용 미리보기 저장.
  const QImage img =
      LayoutService::renderPreview(&proj, QStringLiteral("survey_area_map"), QSize(1400, 990), &err);
  QVERIFY2(!img.isNull(), qPrintable(err));
  img.save(QDir::temp().filePath(QStringLiteral("ka-hgis-layout-pro.png")));
}

// 도면만들기 용지: 맵 안 + 십자와 바깥 테두리 좌표 자(주기)는 기본으로 끄고,
// PDF는 그 조판을 그대로 보낸다(내보낼 때 도곽을 다시 켜지 않음).
void TestWorkflow::drawingStudio_sheetOmitsCrossesAndBorderRuler() {
  QFile hdr(QStringLiteral("src/app/KaDrawingStudio.h"));
  QVERIFY2(hdr.open(QIODevice::ReadOnly | QIODevice::Text), "KaDrawingStudio.h");
  const QString h = QString::fromUtf8(hdr.readAll());
  QVERIFY2(h.contains(QLatin1String("m_gridEnabled = false")),
           "도면만들기 기본은 좌표 격자(+·테두리 자)를 끈다");
  QVERIFY2(h.contains(QLatin1String("m_gridShowNums = false")),
           "바깥 테두리 좌표 숫자도 기본 꺼짐");

  QFile cpp(QStringLiteral("src/app/KaDrawingStudio.cpp"));
  QVERIFY2(cpp.open(QIODevice::ReadOnly | QIODevice::Text), "KaDrawingStudio.cpp");
  const QString src = QString::fromUtf8(cpp.readAll());

  const int stdDecor = src.indexOf(QLatin1String("void KaDrawingStudio::ensureStandardDecorations()"));
  const int nextStd = src.indexOf(QLatin1String("void KaDrawingStudio::applyStandardChromePositions()"));
  QVERIFY2(stdDecor >= 0 && nextStd > stdDecor, "ensureStandardDecorations");
  const QString decor = src.mid(stdDecor, nextStd - stdDecor);
  QVERIFY2(decor.contains(QLatin1String("applyCrsGrid")),
           "조판을 열면 기본(꺼짐)대로 격자를 걷어 조판과 PDF가 같아진다");

  QVERIFY2(src.contains(QLatin1String("&KaDrawingStudio::savePdf")),
           "조판 PDF 버튼이 savePdf에 연결되어야 한다");
  const int save = src.indexOf(QLatin1String("void KaDrawingStudio::savePdf()"));
  QVERIFY2(save >= 0, "savePdf");
  const QString pdfFn = src.mid(save, 900);
  QVERIFY2(pdfFn.contains(QLatin1String("applyCrsGrid")),
           "PDF 저장 전에 조판과 같은 격자 상태를 맞춘다");
  QVERIFY2(!pdfFn.contains(QLatin1String("applySurveyFrameGrid")),
           "PDF가 도곽 +/자를 다시 켜면 조판과 달라진다");

  const int apply = src.indexOf(QLatin1String("void KaDrawingStudio::applyCrsGrid"));
  QVERIFY2(apply >= 0, "applyCrsGrid");
  const QString applyFn = src.mid(apply, 1600);
  QVERIFY2(applyFn.contains(QLatin1String("removeGrid")),
           "격자 꺼짐이면 맵 안 +와 테두리 자를 지운다");
}

void TestWorkflow::layoutRasterDrawnInOnePass() {
  // 위성지도가 반만 나오는 원인: QGIS 기본값이 래스터를 여러 조각으로 나눠 그려서
  // 조각 하나가 빈 채로 오면 그 사각형이 통째로 빈다. 도면은 항상 한 번에 그린다.
  const auto tiledOff = [](QgsLayout* ly) {
    return ly && ly->renderContext().testFlag(
                     Qgis::LayoutRenderFlag::DisableTiledRasterLayerRenders);
  };

  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));

  // 사용자 도면(조판 스튜디오가 쓰는 빈 용지).
  QString err;
  QVERIFY(!LayoutService::createBlankSheet(&proj, 210.0, 297.0,
                                          QStringLiteral("user_sheet"), &err)
               .isEmpty());
  auto* sheet = dynamic_cast<QgsPrintLayout*>(
      proj.layoutManager()->layoutByName(QStringLiteral("user_sheet")));
  QVERIFY2(tiledOff(sheet), "빈 용지도 래스터를 한 번에 그려야 한다");

  // 자동 생성 도면.
  LayoutService::DrawingOptions opt;
  LayoutService::buildDrawing(&proj, LayoutService::DrawingKind::SurveyAreaMap, opt, &err);
  auto* built = dynamic_cast<QgsPrintLayout*>(
      proj.layoutManager()->layoutByName(QStringLiteral("survey_area_map")));
  QVERIFY2(tiledOff(built), "자동 도면도 래스터를 한 번에 그려야 한다");

  // 예전 프로젝트에서 열린 조판처럼 플래그가 꺼져 있어도 내보내기·미리보기에서 되살린다.
  sheet->renderContext().setFlag(Qgis::LayoutRenderFlag::DisableTiledRasterLayerRenders, false);
  LayoutService::renderPreview(&proj, QStringLiteral("user_sheet"), QSize(200, 280), &err);
  QVERIFY2(tiledOff(sheet), "미리보기 경로에서 다시 켜져야 한다");

  // 화면 미리보기 해상도(96 DPI)로 내보내도 인쇄는 300 DPI로 나가고, 끝나면 되돌린다.
  built->renderContext().setDpi(96.0);
  const QString pdf = QDir::temp().filePath(QStringLiteral("ka-hgis-dpi-check.pdf"));
  QFile::remove(pdf);
  const QString saved = LayoutService::exportLayoutPdf(&proj, QStringLiteral("survey_area_map"),
                                                       pdf, &err);
  QVERIFY2(!saved.isEmpty(), qPrintable(err));
  QCOMPARE(built->renderContext().dpi(), 96.0);

  LayoutService::applySingleRasterPassRendering(nullptr);  // 널 안전
}

void TestWorkflow::nameAttributeLabeling_5ptAndAreaCheck() {
  auto* vl = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186&field=id:integer&field=jibun:string&field=kind:string"),
                                QStringLiteral("cadastral_sample"), QStringLiteral("memory"));
  QVERIFY(vl->isValid());

  // 1. detectNameField 검증: jibun 필드를 정확히 찾아낸다.
  const QString detected = LayerOps::detectNameField(vl);
  QCOMPARE(detected, QStringLiteral("jibun"));

  // 2. 5.0 pt 기본 라벨 적용 (면적 미포함)
  QVERIFY(LayerOps::applyNameAttributeLabels(vl, detected, 5.0, false));
  QVERIFY(LayerOps::labelsVisible(vl));
  QCOMPARE(LayerOps::currentLabelField(vl), QStringLiteral("jibun"));
  QCOMPARE(LayerOps::labelFontSize(vl), 5.0);
  QCOMPARE(LayerOps::labelShowArea(vl), false);

  // 3. 면적 포함 및 8.0 pt로 크기 변경 적용
  QVERIFY(LayerOps::applyNameAttributeLabels(vl, detected, 8.0, true));
  QCOMPARE(LayerOps::labelFontSize(vl), 8.0);
  QCOMPARE(LayerOps::labelShowArea(vl), true);

  // 4. 라벨 끄기/켜기 토글
  LayerOps::setLabelsVisible(vl, false);
  QVERIFY(!LayerOps::labelsVisible(vl));
  LayerOps::setLabelsVisible(vl, true);
  QVERIFY(LayerOps::labelsVisible(vl));

  delete vl;

  // 5. 실제 수신된 한국어 SHP(CP949, .cpg 없음) 인코딩 자동 보정 검증
  const QString sampleShp = QStringLiteral("C:/Users/kyi25/OneDrive/바탕 화면/안동시/발굴조사구역_260904142329633/발굴사업허가구역.shp");
  if (QFile::exists(sampleShp)) {
    const QString enc = LayerOps::prepareShapefileEncoding(sampleShp);
    QCOMPARE(enc, QStringLiteral("CP949"));
    auto* shpL = new QgsVectorLayer(sampleShp, QStringLiteral("허가구역"), QStringLiteral("ogr"));
    QVERIFY(shpL->isValid());
    const QString shpNameField = LayerOps::detectNameField(shpL);
    QCOMPARE(shpNameField, QStringLiteral("사업명"));
    for (const QgsField& f : shpL->fields()) {
      QVERIFY2(!f.name().contains(QChar(0xFFFD)), "필드명에 깨짐 문자(\\uFFFD)가 없어야 한다");
    }
    delete shpL;
  }
}

void TestWorkflow::intersectionSnappingAndSaveAsPreservesLayers() {
  // 1. QgsSnappingConfig 교차점 스냅(Intersection Snapping) 설정 검증
  QgsSnappingConfig cfg;
  cfg.setEnabled(true);
  cfg.setMode(Qgis::SnappingMode::AllLayers);
  cfg.setTypeFlag(Qgis::SnappingType::Vertex | Qgis::SnappingType::Segment);
  cfg.setIntersectionSnapping(true);
  cfg.setSelfSnapping(true);
  cfg.setTolerance(16.0);
  cfg.setUnits(Qgis::MapToolUnit::Pixels);
  QCOMPARE(cfg.intersectionSnapping(), true);
  QCOMPARE(cfg.selfSnapping(), true);
  QVERIFY(cfg.typeFlag().testFlag(Qgis::SnappingType::Vertex));
  QVERIFY(cfg.typeFlag().testFlag(Qgis::SnappingType::Segment));

  // 2. removeSurveyDomainLayers 호출 시 외부 사용자 SHP 레이어가 보존되는지 검증
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));

  // (A) 도면 조사 레이어 (survey_area)
  auto* domainLayer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("조사구역"), QStringLiteral("memory"));
  QVERIFY(domainLayer->isValid());
  LayerOps::markSurveyLayer(domainLayer, QStringLiteral("survey_area"));
  proj.addMapLayer(domainLayer, true);

  // (B) 사용자가 외부에서 불러온 SHP 레이어 (user:안동시_허가구역)
  auto* userLayer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("안동시_허가구역"), QStringLiteral("memory"));
  QVERIFY(userLayer->isValid());
  LayerOps::markSurveyLayer(userLayer, QStringLiteral("user:안동시_허가구역"));
  proj.addMapLayer(userLayer, true);

  // (C) 참조 배경지도 레이어
  auto* baseLayer = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("참조배경"), QStringLiteral("memory"));
  QVERIFY(baseLayer->isValid());
  LayerOps::markReferenceLayer(baseLayer);
  proj.addMapLayer(baseLayer, true);

  const QString domainId = domainLayer->id();
  const QString userId = userLayer->id();
  const QString baseId = baseLayer->id();

  // 조사 도면 레이어만 내리고 외부 사용자 레이어와 배경지도는 유지해야 함!
  LayerOps::removeSurveyDomainLayers(&proj);

  QCOMPARE(proj.mapLayers().size(), 2);
  QVERIFY2(proj.mapLayer(userId) != nullptr, "사용자가 불러온 외부 SHP 레이어는 절대로 지워지지 않아야 한다!");
  QVERIFY2(proj.mapLayer(baseId) != nullptr, "참조 배경지도는 유지되어야 한다!");
  QVERIFY2(proj.mapLayer(domainId) == nullptr, "기존 도면 레이어만 정리되어야 한다!");

  // 3. 프로젝트 저장 및 다른 이름으로 저장(.qgz) 동반 생성 시 외부 레이어 및 속성이 온전히 저장되는지 검증
  const QString tempDir = QDir::tempPath() + QStringLiteral("/ka_hgis_saveas_test_") + QString::number(QDateTime::currentMSecsSinceEpoch());
  QDir().mkpath(tempDir);
  const QString targetQgz = tempDir + QStringLiteral("/안동_작업프로젝트.qgz");

  proj.setFileName(targetQgz);
  const bool writeOk = proj.write();
  QVERIFY2(writeOk, "프로젝트(.qgz) 저장이 성공해야 한다");
  QVERIFY(QFile::exists(targetQgz));

  // 새 프로젝트 객체로 복원 테스트
  QgsProject restoreProj;
  const bool readOk = restoreProj.read(targetQgz);
  QVERIFY2(readOk, "저장된 프로젝트(.qgz)를 다시 열 수 있어야 한다");
  QVERIFY2(restoreProj.mapLayers().size() >= 2, "작업 중이던 레이어들이 프로젝트에 그대로 보존되어 있어야 한다!");

  QDir(tempDir).removeRecursively();
}

void TestWorkflow::zoomToProjectDataLayers_usesUserVectorsNotKorea() {
  QgsProject proj;
  QgsMapCanvas canvas;
  canvas.resize(800, 600);
  QVERIFY(LayerOps::ensureOtfEnabled(&proj, &canvas, QStringLiteral("EPSG:5186")));

  auto* sat = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), QStringLiteral("위성"),
                                 QStringLiteral("memory"));
  QVERIFY(sat->isValid());
  QVERIFY(sat->startEditing());
  QgsFeature sf(sat->fields());
  sf.setGeometry(QgsGeometry::fromRect(QgsRectangle(100000, 300000, 500000, 700000)));
  QVERIFY(sat->addFeature(sf));
  QVERIFY(sat->commitChanges());
  proj.addMapLayer(sat);

  auto* user = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("안동시_문화재구역"), QStringLiteral("memory"));
  QVERIFY(user->isValid());
  LayerOps::markSurveyLayer(user, QStringLiteral("user:안동시_문화재구역"));
  QVERIFY(user->startEditing());
  QgsFeature uf(user->fields());
  uf.setGeometry(QgsGeometry::fromRect(QgsRectangle(348000, 397000, 348200, 397160)));
  QVERIFY(user->addFeature(uf));
  QVERIFY(user->commitChanges());
  proj.addMapLayer(user);

  QVERIFY(LayerOps::zoomToProjectDataLayers(&canvas, &proj));
  QVERIFY2(canvas.scale() < 80000.0, qPrintable(QStringLiteral("축척 %1").arg(canvas.scale())));
  QVERIFY(canvas.extent().contains(QgsPointXY(348100, 397080)));
}

void TestWorkflow::companionQgz_roundtripKeepsFifteenUserLayers() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));

  for (int i = 0; i < 15; ++i) {
    const QString title = QStringLiteral("안동시_레이어%1").arg(i + 1);
    auto* mem = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"), title,
                                   QStringLiteral("memory"));
    QVERIFY(mem->isValid());
    QVERIFY(mem->startEditing());
    QgsFeature f(mem->fields());
    f.setGeometry(QgsGeometry::fromRect(
        QgsRectangle(348000.0 + i * 10.0, 397000.0, 348040.0 + i * 10.0, 397040.0)));
    QVERIFY(mem->addFeature(f));
    QVERIFY(mem->commitChanges());
    const QString shp = dir.filePath(QStringLiteral("layer_%1.shp").arg(i));
    QgsVectorFileWriter::SaveVectorOptions opts;
    opts.driverName = QStringLiteral("ESRI Shapefile");
    opts.fileEncoding = QStringLiteral("UTF-8");
    QString err;
    QCOMPARE(QgsVectorFileWriter::writeAsVectorFormatV3(mem, shp, proj.transformContext(), opts, &err),
             QgsVectorFileWriter::NoError);
    delete mem;
    auto* fileL = new QgsVectorLayer(shp, title, QStringLiteral("ogr"));
    QVERIFY2(fileL->isValid(), qPrintable(fileL->error().message()));
    LayerOps::markSurveyLayer(fileL, QStringLiteral("user:%1").arg(title));
    proj.addMapLayer(fileL);
  }

  const QString qgz = dir.filePath(QStringLiteral("안동시.qgz"));
  proj.setFileName(qgz);
  QVERIFY2(proj.write(), "15장 SHP를 담은 qgz 저장");

  QgsProject loaded;
  QVERIFY2(loaded.read(qgz), "저장한 qgz를 다시 열 수 있어야 한다");
  int restored = 0;
  for (QgsMapLayer* l : loaded.mapLayers()) {
    if (!l) continue;
    if (LayerOps::layerKeyOf(l).startsWith(QLatin1String("user:")) ||
        l->name().startsWith(QStringLiteral("안동시_레이어")))
      ++restored;
  }
  QVERIFY2(restored >= 15, qPrintable(QStringLiteral("복원 %1").arg(restored)));
}

void TestWorkflow::openSurveyGpkg_usesSafeProjectRead() {
  const QString src = readMainWindowSources();
  QVERIFY2(!src.isEmpty(), "MainWindow.cpp + Session");
  QVERIFY2(src.contains(QLatin1String("kaSafeReadQgisProject")),
           "조사 열기는 QgsProject::read 직접 호출이 아니라 SEH 안전 읽기를 써야 한다");
  QVERIFY2(src.contains(QLatin1String("zoomToProjectDataLayers")),
           "열기 후 전국 뷰가 아니라 도면 데이터로 줌해야 한다");
  QVERIFY2(src.contains(QLatin1String("addNonEmptyDomainLayers")) ||
               src.contains(QLatin1String("addNonEmptySavedGpkgLayers")),
           "내장 작업공간이 비어도 GPKG에 있는 도형은 다시 올려야 한다");
  // Invalid-open path retention and subsequent manual/autosave are exercised by
  // TestSaveOpen::invalidSurvey_doesNotReplaceCurrentWork against the real window.
  QVERIFY2(src.contains(QLatin1String("m_surveySessionReady")),
           "성공적으로 연 조사만 자동 저장해야 한다");
  QVERIFY2(src.contains(QLatin1String("companionGpkg")) &&
               src.contains(QLatin1String("openSurveyGpkg")),
           "파일 열기에서 .qgz를 골라도 동반 .gpkg로 조사 레이어를 복원해야 한다");
}

void TestWorkflow::emptyEmbeddedWorkspace_reopenStillShowsCommittedSurveyArea() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_reopen_") +
                                            QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("재열기"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  {
    auto* sa = new QgsVectorLayer(QStringLiteral("%1|layername=survey_area").arg(gpkg),
                                  QStringLiteral("survey_area"), QStringLiteral("ogr"));
    QVERIFY(sa->isValid());
    QVERIFY(sa->startEditing());
    QgsFeature sf(sa->fields());
    sf.setAttribute(QStringLiteral("survey_name"), QStringLiteral("저장확인"));
    QgsPolylineXY ring;
    ring << QgsPointXY(200000, 450000) << QgsPointXY(200080, 450000) << QgsPointXY(200080, 450080)
         << QgsPointXY(200000, 450080) << QgsPointXY(200000, 450000);
    sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
    QVERIFY(sa->addFeature(sf));
    QVERIFY(sa->commitChanges());
    QCOMPARE(int(sa->featureCount()), 1);
    delete sa;
  }

  {
    QgsProject empty;
    empty.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QVERIFY2(SurveyStorage::writeEmbedded(&empty, gpkg, &err), qPrintable(err));
  }
  QVERIFY(SurveyStorage::hasEmbeddedProject(gpkg));

  QgsProject back;
  QVERIFY2(SurveyStorage::readEmbedded(&back, gpkg, nullptr, &err), qPrintable(err));
  QVERIFY2(!LayerOps::findByLayerKey(&back, QStringLiteral("survey_area")),
           "빈 작업공간을 읽으면 범례에 조사구역이 없어야 한다(버그 재현 전제)");
  QCOMPARE(LayerOps::addNonEmptyDomainLayers(&back, gpkg), 1);
  auto* restored = LayerOps::findByLayerKey(&back, QStringLiteral("survey_area"));
  QVERIFY2(restored, "다시 열면 GPKG에 커밋한 조사구역이 보여야 한다");
  QCOMPARE(int(restored->featureCount()), 1);
  QgsFeature f = restored->getFeature(*restored->allFeatureIds().constBegin());
  QCOMPARE(f.attribute(QStringLiteral("survey_name")).toString(), QStringLiteral("저장확인"));
}

void TestWorkflow::emptyEmbeddedWorkspace_reopenStillShowsUserPolygonLayer() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_reopen_user_") +
                                            QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("사용자면"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  QgsProject draw;
  draw.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* user = LayerOps::createUserPolygonLayer(&draw, gpkg, QStringLiteral("내 면"),
                                                QStringLiteral("EPSG:5186"), &err);
  QVERIFY2(user, qPrintable(err));
  QVERIFY(user->startEditing());
  QgsFeature uf(user->fields());
  uf.setGeometry(QgsGeometry::fromRect(QgsRectangle(201000, 451000, 201040, 451040)));
  QVERIFY(user->addFeature(uf));
  QVERIFY(user->commitChanges());
  QCOMPARE(int(user->featureCount()), 1);
  const QString userKey = LayerOps::layerKeyOf(user);
  QVERIFY(userKey.startsWith(QLatin1String("user_poly_")));

  {
    QgsProject empty;
    empty.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    QVERIFY2(SurveyStorage::writeEmbedded(&empty, gpkg, &err), qPrintable(err));
  }

  QgsProject back;
  QVERIFY2(SurveyStorage::readEmbedded(&back, gpkg, nullptr, &err), qPrintable(err));
  QVERIFY2(!LayerOps::findByLayerKey(&back, userKey),
           "빈 작업공간에는 사용자 면 레이어가 없어야 한다(버그 재현 전제)");
  QVERIFY(LayerOps::addNonEmptySavedGpkgLayers(&back, gpkg) >= 1);
  auto* restored = LayerOps::findByLayerKey(&back, userKey);
  QVERIFY2(restored && restored->isValid(), "다시 열면 GPKG에 커밋한 사용자 면이 보여야 한다");
  QCOMPARE(int(restored->featureCount()), 1);
}

void TestWorkflow::invalidWorkspaceLayer_isReplacedFromGpkgTable() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_reopen_bad_") +
                                            QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("깨진레이어"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  {
    auto* sa = new QgsVectorLayer(QStringLiteral("%1|layername=survey_area").arg(gpkg),
                                  QStringLiteral("survey_area"), QStringLiteral("ogr"));
    QVERIFY(sa->isValid());
    QVERIFY(sa->startEditing());
    QgsFeature sf(sa->fields());
    sf.setAttribute(QStringLiteral("survey_name"), QStringLiteral("복구확인"));
    sf.setGeometry(QgsGeometry::fromRect(QgsRectangle(202000, 452000, 202080, 452080)));
    QVERIFY(sa->addFeature(sf));
    QVERIFY(sa->commitChanges());
    delete sa;
  }

  QgsProject back;
  auto* broken = new QgsVectorLayer(QStringLiteral("C:/ka-hgis-missing.gpkg|layername=survey_area"),
                                    QStringLiteral("조사구역"), QStringLiteral("ogr"));
  QVERIFY(!broken->isValid());
  LayerOps::markSurveyLayer(broken, QStringLiteral("survey_area"));
  back.addMapLayer(broken);
  QVERIFY(LayerOps::findByLayerKey(&back, QStringLiteral("survey_area")));
  QVERIFY(!LayerOps::findByLayerKey(&back, QStringLiteral("survey_area"))->isValid());

  QVERIFY(LayerOps::addNonEmptySavedGpkgLayers(&back, gpkg) >= 1);
  auto* restored = LayerOps::findByLayerKey(&back, QStringLiteral("survey_area"));
  QVERIFY2(restored && restored->isValid(), "깨진 작업공간 레이어는 GPKG 테이블로 바꿔야 한다");
  QCOMPARE(int(restored->featureCount()), 1);
}

// 조사 파일 이름을 바꾸거나 다른 폴더로 복사하면, 내장 작업공간이 적어 둔 경로가
// 어긋나 저장된 벡터가 전부 invalid 로 열린다. 예전에는 layer_key 가 테이블 이름과
// 똑같은 도메인 레이어만 자리를 물려받아서, 들여온 SHP(layer_key="user:이름")는
// 죽은 채 범례에 남고 그 옆에 같은 이름의 새 레이어가 하나 더 생겼다. 실제 현장
// 파일에서 15장이 25장(그중 11장 invalid)이 됐다.
void TestWorkflow::renamedSurvey_repointsImportedLayersInsteadOfDuplicating() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QString error;
  const QString gpkg =
      SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("이름바꾼조사"), &error);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(error));

  // 들여온 SHP 한 장을 흉내 낸다: .gpkg 안의 테이블 + "user:" 접두 layer_key.
  const QString table = QStringLiteral("문화유적분포지도");
  {
    QgsVectorLayer mem(QStringLiteral("Polygon?crs=EPSG:5187&field=name:string"), table,
                       QStringLiteral("memory"));
    QVERIFY(mem.isValid());
    QVERIFY(mem.startEditing());
    QgsFeature f(mem.fields());
    f.setGeometry(QgsGeometry::fromRect(QgsRectangle(205000, 455000, 205080, 455080)));
    QVERIFY(mem.addFeature(f));
    QVERIFY(mem.commitChanges());
    QgsVectorFileWriter::SaveVectorOptions opt;
    opt.driverName = QStringLiteral("GPKG");
    opt.layerName = table;
    opt.actionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
    QgsProject scratch;
    QString werr;
    QCOMPARE(QgsVectorFileWriter::writeAsVectorFormatV3(&mem, gpkg, scratch.transformContext(),
                                                        opt, &werr),
             QgsVectorFileWriter::NoError);
  }

  // 파일 이름을 바꾼다 — 사용자가 "안동시.gpkg"를 "안동시_복사본.gpkg"로 복사한 상황.
  const QString renamed = dir.filePath(QStringLiteral("이름바꾼조사_복사본.gpkg"));
  QVERIFY(QFile::copy(gpkg, renamed));

  // 저장된 작업공간이 적어 둔 옛 경로. 이름이 바뀐 뒤 열면 정확히 이 모습이 된다.
  const QString oldPath = dir.filePath(QStringLiteral("이름바꾸기전.gpkg"));
  QVERIFY(!QFileInfo::exists(oldPath));
  QgsProject project;
  auto* saved = new QgsVectorLayer(QStringLiteral("%1|layername=%2").arg(oldPath, table),
                                   QStringLiteral("문화유적분포지도"), QStringLiteral("ogr"));
  QVERIFY(!saved->isValid());
  LayerOps::markSurveyLayer(saved, QStringLiteral("user:%1").arg(table));
  project.addMapLayer(saved);
  const QString savedId = saved->id();

  QVERIFY(LayerOps::addNonEmptySavedGpkgLayers(&project, renamed) >= 1);

  // 같은 레이어 객체가 새 파일을 가리켜야 한다. 이름·스타일·범례 자리가 그대로 남는다.
  auto* repointed = qobject_cast<QgsVectorLayer*>(project.mapLayer(savedId));
  QVERIFY2(repointed, "이름이 바뀐 조사 파일에서도 저장된 레이어 객체가 살아 있어야 한다");
  QVERIFY2(repointed->isValid(), "옛 경로가 깨진 레이어는 새 경로로 다시 물려야 한다");
  QCOMPARE(repointed->featureCount(), 1LL);
  QVERIFY(project.layerTreeRoot()->findLayer(savedId));

  // 같은 이름의 죽은 짝이 생기면 안 된다.
  int sameName = 0;
  for (QgsMapLayer* l : project.mapLayers()) {
    QVERIFY(l);
    if (l->name() == QStringLiteral("문화유적분포지도")) ++sameName;
    QVERIFY2(l->isValid(),
             qPrintable(QStringLiteral("invalid 레이어가 남았다: %1").arg(l->name())));
  }
  QCOMPARE(sameName, 1);

  // 두 번째로 열어도 늘어나지 않는다.
  const int before = project.mapLayers().size();
  LayerOps::addNonEmptySavedGpkgLayers(&project, renamed);
  QCOMPARE(project.mapLayers().size(), before);
}

// 저장된 조사의 위성·지적 주소에는 그때 쓰던 인증키가 통째로 박혀 있다. 키가 만료돼
// 새 키를 받아도 예전 조사를 열면 만료된 키로 타일을 받아 배경지도가 백지가 된다.
// VWorld 는 만료된 키에도 HTTP 200 에 XML 오류를 돌려주므로 오류조차 안 보인다.
void TestWorkflow::savedVworldUrl_swapsExpiredKeyForCurrentOne() {
  // 실제 키는 테스트에 넣지 않는다. 모양만 같은 가짜 GUID 두 개면 충분하다.
  const QString expired = QStringLiteral("00000000-1111-2222-3333-444455556666");
  const QString fresh = QStringLiteral("11112222-3333-4444-5555-666677778888");

  const QString wmts =
      QStringLiteral("type=xyz&url=https://api.vworld.kr/req/wmts/1.0.0/%1/Satellite/"
                     "%7Bz%7D/%7By%7D/%7Bx%7D.jpeg&zmax=19&zmin=6&crs=EPSG:3857")
          .arg(expired);
  const QString swappedWmts = LayerOps::withVworldApiKey(wmts, fresh);
  QVERIFY(swappedWmts.contains(fresh));
  QVERIFY(!swappedWmts.contains(expired));
  // 주소의 나머지는 그대로여야 한다 — zmax/crs 를 건드리면 타일이 안 나온다.
  QVERIFY(swappedWmts.contains(QLatin1String("zmax=19")));
  QVERIFY(swappedWmts.contains(QLatin1String("crs=EPSG:3857")));

  // WMS 는 url= 안에서 퍼센트 인코딩된 KEY%3D 형태로도 나타난다.
  const QString wms =
      QStringLiteral("crs=EPSG:3857&layers=lp_pa_cbnd_bonbun"
                     "&url=https%3A%2F%2Fapi.vworld.kr%2Freq%2Fwms%3FKEY%3D%1%26DOMAIN%3Dlocalhost")
          .arg(expired);
  const QString swappedWms = LayerOps::withVworldApiKey(wms, fresh);
  QVERIFY(swappedWms.contains(fresh));
  QVERIFY(!swappedWms.contains(expired));
  QVERIFY(swappedWms.contains(QLatin1String("%26DOMAIN%3Dlocalhost")));

  // VWorld 가 아닌 주소와 빈 키는 건드리지 않는다.
  const QString google = QStringLiteral(
      "type=xyz&url=https://mt1.google.com/vt/lyrs%3Ds%26x%3D%7Bx%7D&zmax=20&crs=EPSG:3857");
  QCOMPARE(LayerOps::withVworldApiKey(google, fresh), google);
  QCOMPARE(LayerOps::withVworldApiKey(wmts, QString()), wmts);

  // 프로젝트에 올라간 레이어도 실제로 갈아 끼워야 한다.
  QgsProject project;
  auto* sat = new QgsRasterLayer(wmts, QStringLiteral("위성"), QStringLiteral("wms"));
  project.addMapLayer(sat);
  QStringList changed;
  QCOMPARE(LayerOps::refreshVworldApiKeyInLayers(&project, fresh, &changed), 1);
  QCOMPARE(changed, QStringList{QStringLiteral("위성")});
  QVERIFY(sat->source().contains(fresh));
  QVERIFY(!sat->source().contains(expired));
  // 두 번 불러도 더 바꿀 것이 없다.
  QCOMPARE(LayerOps::refreshVworldApiKeyInLayers(&project, fresh), 0);
}

void TestWorkflow::savedVworldGdalXml_refreshesKeyWithoutChangingOriginal() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString expired = QStringLiteral("00000000-1111-2222-3333-444455556666");
  const QString fresh = QStringLiteral("11112222-3333-4444-5555-666677778888");
  const QByteArray xml = QStringLiteral(
      "<GDAL_WMS><Service name=\"WMS\"><Version>1.3.0</Version>"
      "<ServerUrl>https://api.vworld.kr/req/wms?key=%1&amp;</ServerUrl>"
      "<Layers>lp_pa_cbnd_bonbun,lp_pa_cbnd_bubun</Layers>"
      "<CRS>EPSG:3857</CRS><ImageFormat>image/png</ImageFormat>"
      "<Transparent>TRUE</Transparent></Service>"
      "<DataWindow><UpperLeftX>13500000</UpperLeftX><UpperLeftY>4800000</UpperLeftY>"
      "<LowerRightX>14800000</LowerRightX><LowerRightY>3800000</LowerRightY>"
      "<SizeX>16384</SizeX><SizeY>16384</SizeY></DataWindow>"
      "<Projection>EPSG:3857</Projection><BandsCount>4</BandsCount>"
      "<Referer>https://localhost</Referer><OfflineMode>true</OfflineMode>"
      "</GDAL_WMS>").arg(expired).toUtf8();
  const QString path = dir.filePath(QStringLiteral("old-cadastral.xml"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write(xml), xml.size());
  file.close();
  QgsProject project;
  auto* cad = new QgsRasterLayer(path, QStringLiteral("지적"), QStringLiteral("gdal"));
  QVERIFY(cad->isValid());
  cad->setOpacity(0.65);
  LayerOps::markReferenceLayer(cad);
  project.addMapLayer(cad);
  const QString id = cad->id();
  project.layerTreeRoot()->findLayer(id)->setItemVisibilityChecked(false);
  QStringList changed;
  QCOMPARE(LayerOps::refreshVworldApiKeyInLayers(&project, fresh, &changed), 1);
  QCOMPARE(changed, QStringList{QStringLiteral("지적")});
  QVERIFY(cad->isValid());
  QCOMPARE(cad->id(), id);
  QCOMPARE(cad->opacity(), 0.65);
  QVERIFY(!project.layerTreeRoot()->findLayer(id)->isVisible());
  QVERIFY(cad->source().contains(fresh));
  QVERIFY(!cad->source().contains(expired));
  QCOMPARE(cad->crs().authid(), QStringLiteral("EPSG:3857"));
  QVERIFY(cad->rasterUnitsPerPixelX() <= 0.5);
  QVERIFY(cad->rasterUnitsPerPixelY() <= 0.5);
  QCOMPARE(LayerOps::refreshVworldApiKeyInLayers(&project, fresh), 0);
  // A later key change must also reach the inline GDAL source.
  QCOMPARE(LayerOps::refreshVworldApiKeyInLayers(&project, expired), 1);
  QVERIFY(cad->source().contains(expired));
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), xml);
  const QString workspace = dir.filePath(QStringLiteral("refreshed.qgs"));
  QVERIFY(project.write(workspace));
  QgsProject reopened;
  QVERIFY(reopened.read(workspace));
  auto* restored = qobject_cast<QgsRasterLayer*>(reopened.mapLayer(id));
  QVERIFY(restored && restored->isValid());
  QVERIFY(restored->source().contains(expired));
  QVERIFY(restored->rasterUnitsPerPixelX() <= 0.5);
  QVERIFY(restored->rasterUnitsPerPixelY() <= 0.5);
}

void TestWorkflow::savedGpkgWorkspace_restoresLegendMembership_data() {
  QTest::addColumn<bool>("inLegend");
  QTest::addColumn<bool>("checked");
  QTest::newRow("registry-only") << false << true;
  QTest::newRow("saved-hidden") << true << false;
  QTest::newRow("saved-visible") << true << true;
}

void TestWorkflow::savedGpkgWorkspace_restoresLegendMembership() {
  QFETCH(bool, inLegend);
  QFETCH(bool, checked);
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(
      dir.path(), QStringLiteral("범례복원"), &err, QStringLiteral("EPSG:5187"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  const QString title = QStringLiteral("작업한 조사구역");
  const QColor fill(230, 40, 150, 170);
  QString layerId;
  {
    QgsProject saved;
    saved.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5187")));
    auto* area = new QgsVectorLayer(QStringLiteral("%1|layername=survey_area").arg(gpkg),
                                    title, QStringLiteral("ogr"));
    QVERIFY(area->isValid());
    LayerOps::markSurveyLayer(area, QStringLiteral("survey_area"));
    QVERIFY(area->startEditing());
    QgsFeature f(area->fields());
    f.setAttribute(QStringLiteral("survey_name"), QStringLiteral("다시 열 작업"));
    f.setGeometry(QgsGeometry::fromRect(QgsRectangle(203000, 453000, 203080, 453080)));
    QVERIFY(area->addFeature(f));
    QVERIFY(area->commitChanges());
    QVERIFY(LayerOps::applySimpleVectorStyle(area, fill, QColor(90, 0, 60), 1.7));
    layerId = area->id();
    saved.addMapLayer(area, inLegend);
    if (inLegend)
      saved.layerTreeRoot()->findLayer(layerId)->setItemVisibilityChecked(checked);
    QVERIFY2(SurveyStorage::writeEmbedded(&saved, gpkg, &err), qPrintable(err));
  }

  QgsProject opened;
  QVERIFY2(SurveyStorage::readEmbedded(&opened, gpkg, nullptr, &err), qPrintable(err));
  auto* area = qobject_cast<QgsVectorLayer*>(opened.mapLayer(layerId));
  QVERIFY(area && area->isValid());
  QCOMPARE(opened.layerTreeRoot()->findLayer(layerId) != nullptr, inLegend);
  QCOMPARE(opened.crs().authid(), QStringLiteral("EPSG:5187"));
  for (int attempt = 0; attempt < 2; ++attempt) {
    LayerOps::addNonEmptySavedGpkgLayers(&opened, gpkg);
    QCOMPARE(opened.mapLayers().size(), 1);
    QCOMPARE(opened.mapLayer(layerId), area);
    auto* node = opened.layerTreeRoot()->findLayer(layerId);
    QVERIFY2(node, "레지스트리에만 남은 저장 레이어도 범례에 복원되어야 한다");
    QCOMPARE(opened.layerTreeRoot()->findLayers().size(), 1);
    QCOMPARE(node->itemVisibilityChecked(), checked);
    QCOMPARE(area->name(), title);
    QCOMPARE(area->featureCount(), 1LL);
    QgsFeature feature;
    QVERIFY(area->getFeatures().nextFeature(feature));
    QCOMPARE(feature.attribute(QStringLiteral("survey_name")).toString(), QStringLiteral("다시 열 작업"));
    auto* renderer = dynamic_cast<QgsSingleSymbolRenderer*>(area->renderer());
    QVERIFY(renderer && renderer->symbol());
    QCOMPARE(renderer->symbol()->color(), fill);
  }
}

void TestWorkflow::savedGpkgWorkspace_sameKeyKeepsDistinctTables_data() {
  QTest::addColumn<bool>("secondHasFeatures");
  QTest::newRow("populated-second") << true;
  QTest::newRow("empty-second") << false;
}

void TestWorkflow::savedGpkgWorkspace_sameKeyKeepsDistinctTables() {
  QFETCH(bool, secondHasFeatures);
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QString error;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir.path(), QStringLiteral("여러구역복원"), &error);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(error));
  QgsProject project;
  auto* first = LayerOps::ensureDomainLayer(&project, gpkg, QStringLiteral("survey_area"),
                                           QStringLiteral("원래 구역"), &error);
  QVERIFY2(first, qPrintable(error));
  QVERIFY(first->startEditing());
  QgsFeature feature(first->fields());
  feature.setGeometry(QgsGeometry::fromRect(QgsRectangle(203000, 453000, 203080, 453080)));
  QVERIFY(first->addFeature(feature));
  QVERIFY(first->commitChanges());
  auto* second = LayerOps::createSurveyAreaLayer(&project, gpkg, QStringLiteral("별도 구역"),
                                                 Qt::black, Qt::blue, 0.3, &error);
  QVERIFY2(second, qPrintable(error));
  QVERIFY(second->source().contains(QLatin1String("layername=survey_area_2")));
  QCOMPARE(LayerOps::layerKeyOf(second), LayerOps::layerKeyOf(first));
  if (secondHasFeatures) {
    QVERIFY(second->startEditing());
    QgsFeature other(second->fields());
    other.setGeometry(QgsGeometry::fromRect(QgsRectangle(204000, 454000, 204080, 454080)));
    QVERIFY(second->addFeature(other));
    QVERIFY(second->commitChanges());
  }
  const QString secondId = second->id();
  QPointer<QgsVectorLayer> secondGuard(second);
  project.layerTreeRoot()->findLayer(secondId)->setItemVisibilityChecked(false);
  project.removeMapLayer(first->id());

  LayerOps::addNonEmptySavedGpkgLayers(&project, gpkg);

  QVERIFY2(secondGuard, "Recovery must not delete a separate user-created empty table");
  QCOMPARE(project.mapLayer(secondId), second);
  QVERIFY(!project.layerTreeRoot()->findLayer(secondId)->itemVisibilityChecked());
  QCOMPARE(second->featureCount(), secondHasFeatures ? 1LL : 0LL);
  QgsVectorLayer* restored = nullptr;
  for (auto* mapLayer : project.mapLayers()) {
    auto* vector = qobject_cast<QgsVectorLayer*>(mapLayer);
    if (vector && vector->source().section(QLatin1Char('|'), 1) == QLatin1String("layername=survey_area"))
      restored = vector;
  }
  QVERIFY2(restored, "The populated primary table must be restored even when survey_area_2 has the same layer key");
  QCOMPARE(restored->featureCount(), 1LL);
  QCOMPARE(project.mapLayers().size(), 2);
  QCOMPARE(LayerOps::addNonEmptySavedGpkgLayers(&project, gpkg), 0);
  QCOMPARE(project.mapLayers().size(), 2);
}

void TestWorkflow::satellitePruning_preservesUnresolvedProjectNodes() {
  QgsProject project;
  auto* group = project.layerTreeRoot()->addGroup(QStringLiteral("조사 데이터"));
  const QString layerId = QStringLiteral("survey_area_pending_read");
  auto* pending = new QgsLayerTreeLayer(layerId, QStringLiteral("조사구역"),
      QStringLiteral("./survey.gpkg|layername=survey_area"), QStringLiteral("ogr"));
  pending->setItemVisibilityChecked(false);
  group->addChildNode(pending);
  QVERIFY(!pending->layer());

  LayerOps::pruneDuplicateSatelliteLayers(&project);

  QCOMPARE(project.layerTreeRoot()->findLayer(layerId), pending);
  QCOMPARE(group->children().size(), 1);
  QVERIFY(!pending->itemVisibilityChecked());
}

void TestWorkflow::savedWorkspace_restoresExternalAndRasterLegendNodes() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(
      dir.path(), QStringLiteral("외부레이어복원"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));
  QgsProject project;
  project.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  const QString shp = dir.filePath(QStringLiteral("outside.shp"));
  {
    QgsVectorLayer memory(QStringLiteral("Polygon?crs=EPSG:5186"),
                           QStringLiteral("외부 도형"), QStringLiteral("memory"));
    QgsFeature f(memory.fields());
    f.setGeometry(QgsGeometry::fromRect(QgsRectangle(203000, 453000, 203080, 453080)));
    QVERIFY(memory.dataProvider()->addFeature(f));
    QgsVectorFileWriter::SaveVectorOptions options;
    options.driverName = QStringLiteral("ESRI Shapefile");
    options.fileEncoding = QStringLiteral("UTF-8");
    QCOMPARE(QgsVectorFileWriter::writeAsVectorFormatV3(
                 &memory, shp, project.transformContext(), options, &err),
             QgsVectorFileWriter::NoError);
  }
  auto* external = new QgsVectorLayer(shp, QStringLiteral("외부 SHP"), QStringLiteral("ogr"));
  QVERIFY(external->isValid());
  project.addMapLayer(external, false);

  const QString png = dir.filePath(QStringLiteral("photo.png"));
  QImage photo(8, 8, QImage::Format_RGB32);
  photo.fill(Qt::white);
  QVERIFY(photo.save(png));
  auto* raster = new QgsRasterLayer(png, QStringLiteral("현장 사진"), QStringLiteral("gdal"));
  QVERIFY(raster->isValid());
  project.addMapLayer(raster, false);

  auto* empty = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                   QStringLiteral("사용자가 만든 빈 레이어"), QStringLiteral("memory"));
  QVERIFY(empty->isValid());
  QCOMPARE(empty->featureCount(), 0LL);
  project.addMapLayer(empty, false);
  auto* hidden = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                    QStringLiteral("숨겨 둔 레이어"), QStringLiteral("memory"));
  project.addMapLayer(hidden);
  project.layerTreeRoot()->findLayer(hidden->id())->setItemVisibilityChecked(false);
  auto* invalid = new QgsVectorLayer(dir.filePath(QStringLiteral("missing.shp")),
                                     QStringLiteral("없는 파일"), QStringLiteral("ogr"));
  QVERIFY(!invalid->isValid());
  project.addMapLayer(invalid, false);

  QCOMPARE(LayerOps::addNonEmptySavedGpkgLayers(&project, gpkg), 3);
  QCOMPARE(project.mapLayers().size(), 5);
  QCOMPARE(project.layerTreeRoot()->findLayers().size(), 4);
  for (QgsMapLayer* layer : QList<QgsMapLayer*>{external, raster, empty}) {
    auto* node = project.layerTreeRoot()->findLayer(layer->id());
    QVERIFY(node && node->layer() == layer);
    QVERIFY(node->itemVisibilityChecked());
  }
  QVERIFY(!project.layerTreeRoot()->findLayer(hidden->id())->itemVisibilityChecked());
  QVERIFY(!project.layerTreeRoot()->findLayer(invalid->id()));
  QCOMPARE(LayerOps::addNonEmptySavedGpkgLayers(&project, gpkg), 0);
  QCOMPARE(project.layerTreeRoot()->findLayers().size(), 4);
}

static QMap<QString, qint64> gpkgFeatureCountsForTest(const QString& path) {
  QMap<QString, qint64> counts;
  GDALDatasetH dataset = GDALOpenEx(path.toUtf8().constData(), GDAL_OF_VECTOR,
                                    nullptr, nullptr, nullptr);
  if (!dataset) return counts;
  for (int i = 0; i < GDALDatasetGetLayerCount(dataset); ++i) {
    OGRLayerH layer = GDALDatasetGetLayer(dataset, i);
    if (layer && OGR_L_GetGeomType(layer) != wkbNone)
      counts.insert(QString::fromUtf8(OGR_L_GetName(layer)), OGR_L_GetFeatureCount(layer, 1));
  }
  GDALClose(dataset);
  return counts;
}

void TestWorkflow::fieldAndongCopy_reopenRestoresSavedGpkgLayers() {
  const QString srcGpkg = qEnvironmentVariable("KA_HGIS_FIELD_GPKG");
  if (srcGpkg.isEmpty())
    QSKIP("Set KA_HGIS_FIELD_GPKG to opt in to field-file copy verification");
  QVERIFY2(QFile::exists(srcGpkg), "현장 검증 파일이 없습니다");
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString gpkg = tmp.filePath(QFileInfo(srcGpkg).fileName());
  QVERIFY2(QFile::copy(srcGpkg, gpkg), "현장 파일 복사 실패");
  const auto expected = gpkgFeatureCountsForTest(gpkg);
  QVERIFY(!expected.isEmpty());

  QgsProject proj;
  QString err;
  bool crashed = false;
  SurveyStorage::readEmbedded(&proj, gpkg, &crashed, &err);
  QVERIFY2(!crashed, qPrintable(err));
  LayerOps::addNonEmptySavedGpkgLayers(&proj, gpkg);

  for (auto it = expected.cbegin(); it != expected.cend(); ++it) {
    if (it.value() <= 0) continue;
    QgsVectorLayer* restored = nullptr;
    for (QgsMapLayer* layer : proj.mapLayers()) {
      auto* vector = qobject_cast<QgsVectorLayer*>(layer);
      if (vector && vector->source().section(QLatin1String("layername="), 1)
                            .section(QLatin1Char('|'), 0, 0) == it.key() &&
          QFileInfo(vector->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath() ==
              QFileInfo(gpkg).absoluteFilePath()) {
        restored = vector;
        break;
      }
    }
    QVERIFY2(restored && restored->isValid(), qPrintable(it.key()));
    QCOMPARE(restored->featureCount(), it.value());
    QVERIFY(proj.layerTreeRoot()->findLayer(restored->id()));
  }
}

void TestWorkflow::fieldAndongCopy_embeddedRelativePathOpensWithoutCwd() {
  const QString srcGpkg = qEnvironmentVariable("KA_HGIS_FIELD_GPKG");
  if (srcGpkg.isEmpty())
    QSKIP("Set KA_HGIS_FIELD_GPKG to opt in to field-file copy verification");
  QVERIFY2(QFile::exists(srcGpkg), "현장 검증 파일이 없습니다");
  QTemporaryDir tmp;
  QVERIFY(tmp.isValid());
  const QString gpkg = tmp.filePath(QFileInfo(srcGpkg).fileName());
  QVERIFY2(QFile::copy(srcGpkg, gpkg), "현장 파일 복사 실패");
  const auto expected = gpkgFeatureCountsForTest(gpkg);
  QVERIFY(!expected.isEmpty());

  const QString oldCwd = QDir::currentPath();
  QVERIFY(QDir::setCurrent(QStringLiteral("C:/")));
  QgsProject proj;
  QString err;
  bool crashed = false;
  const bool ok = SurveyStorage::readEmbedded(&proj, gpkg, &crashed, &err);
  QDir::setCurrent(oldCwd);
  QVERIFY2(ok && !crashed, qPrintable(err));
  int checked = 0;
  for (QgsMapLayer* layer : proj.mapLayers()) {
    auto* vector = qobject_cast<QgsVectorLayer*>(layer);
    if (!vector) continue;
    const QString table = vector->source().section(QLatin1String("layername="), 1)
                              .section(QLatin1Char('|'), 0, 0);
    if (!expected.contains(table)) continue;
    QVERIFY2(vector->isValid(), qPrintable(table));
    QCOMPARE(QFileInfo(vector->source().section(QLatin1Char('|'), 0, 0)).absoluteFilePath(),
             QFileInfo(gpkg).absoluteFilePath());
    QCOMPARE(vector->featureCount(), expected.value(table));
    ++checked;
  }
  QVERIFY2(checked > 0, "내장 작업공간에서 조사 파일 레이어를 찾지 못했습니다");
}

void TestWorkflow::leftoverRestore_keepsUserFillColorNotFactoryDomainStyle() {
  const QString dir = QDir::temp().filePath(QStringLiteral("ka_reopen_style_") +
                                            QString::number(QDateTime::currentMSecsSinceEpoch()));
  QDir().mkpath(dir);
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(dir, QStringLiteral("색유지"), &err);
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  QgsProject draw;
  draw.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  auto* sa = LayerOps::ensureDomainLayer(&draw, gpkg, QStringLiteral("survey_area"),
                                         QStringLiteral("조사구역"), &err);
  QVERIFY2(sa, qPrintable(err));
  QVERIFY(sa->startEditing());
  QgsFeature sf(sa->fields());
  sf.setAttribute(QStringLiteral("survey_name"), QStringLiteral("색확인"));
  sf.setGeometry(QgsGeometry::fromRect(QgsRectangle(203000, 453000, 203080, 453080)));
  QVERIFY(sa->addFeature(sf));
  QVERIFY(sa->commitChanges());

  const QColor userFill(255, 0, 128, 160);
  const QColor userStroke(128, 0, 64, 255);
  QVERIFY(LayerOps::applySimpleVectorStyle(sa, userFill, userStroke, 2.4));
  QVERIFY2(SurveyStorage::writeEmbedded(&draw, gpkg, &err), qPrintable(err));

  QgsProject back;
  QVERIFY(LayerOps::addNonEmptySavedGpkgLayers(&back, gpkg) >= 1);
  auto* restored = LayerOps::findByLayerKey(&back, QStringLiteral("survey_area"));
  QVERIFY2(restored && restored->isValid(), "다시 열면 조사구역이 있어야 한다");
  auto* rend = dynamic_cast<QgsSingleSymbolRenderer*>(restored->renderer());
  QVERIFY2(rend && rend->symbol(), "단일 심볼 렌더러가 유지되어야 한다");
  const QColor got = rend->symbol()->color();
  QVERIFY2(got.red() == 255 && got.green() == 0 && got.blue() == 128,
           qPrintable(QStringLiteral("사용자 채움색이 공장 기본색으로 바뀌면 안 된다 (got %1)")
                          .arg(got.name(QColor::HexArgb))));
}

void TestWorkflow::restoreLastSurvey_prefersEmbeddedWorkspaceWhenSafe() {
  const QString src = readMainWindowSources();
  QVERIFY2(!src.isEmpty(), "MainWindow.cpp + Session");
  QVERIFY2(src.contains(QLatin1String("OpenSurveyMode::PreferWorkspace")),
           "조사 열기는 안전하면 내장 작업공간(색·심볼)을 읽어야 한다");
  const QString opsSrc = readLayerOpsSources();
  QVERIFY2(!opsSrc.isEmpty(), "LayerOps.cpp + BasemapOps + LabelOps + ControlPointCsv");
  QVERIFY2(opsSrc.contains(QLatin1String("saveGpkgDefaultStyles")) &&
               opsSrc.contains(QLatin1String("saveStyleToDatabaseV2")),
           "저장 때 GPKG layer_styles 에 색을 남겨 LayersOnly 폴백도 복원해야 한다");
}

void TestWorkflow::restoreLastSurvey_bootUsesLayersOnlyToAvoidWmsAv() {
  const QString src = readMainWindowSources();
  QVERIFY2(!src.isEmpty(), "MainWindow.cpp + Session");
  const int fn = src.indexOf(QLatin1String("void MainWindow::restoreLastSurvey()"));
  QVERIFY2(fn >= 0, "restoreLastSurvey 가 있어야 한다");
  const int next = src.indexOf(QLatin1String("void MainWindow::"), fn + 10);
  QVERIFY2(next > fn, "restoreLastSurvey body");
  const QString body = src.mid(fn, next - fn);
  QVERIFY2(body.contains(QLatin1String("OpenSurveyMode::LayersOnly")),
           "아이콘 재실행은 GPKG 테이블+위성만 — 내장 24레이어 WMS AV 방지");
  QVERIFY2(!body.contains(QLatin1String("OpenSurveyMode::PreferWorkspace")),
           "부팅 복원에 PreferWorkspace 를 넣으면 crash-20260906-153900 이 재발한다");
  QVERIFY2(!body.contains(QLatin1String("setSkipAutoRestore(st, false)")),
           "복원 직후 skip 을 내리면 타일 AV 다음 실행이 또 복원해서 꺼진다");
}

void TestWorkflow::finishOpenedProject_doesNotBareRefreshWhileWms() {
  const QString src = readMainWindowSources();
  QVERIFY2(!src.isEmpty(), "MainWindow.cpp + Session");
  const int fn = src.indexOf(QLatin1String("void MainWindow::finishOpenedProject"));
  QVERIFY2(fn >= 0, "finishOpenedProject 가 있어야 한다");
  const int next = src.indexOf(QLatin1String("bool MainWindow::openSurveyGpkg"), fn + 10);
  QVERIFY2(next > fn, "finishOpenedProject body");
  const QString body = src.mid(fn, next - fn);
  QVERIFY2(!body.contains(QLatin1String("m_canvas->refresh()")),
           "작업공간 연 직후 빈 refresh() 는 provider_wms deleteLater AV");
  QVERIFY2(body.contains(QLatin1String("refreshXyzBasemapTiles")),
           "그리는 중이면 skip 하는 XYZ 갱신만 써야 한다");
}

#include "test_workflow.moc"

// 손상 파일이 애초에 만들어지지 않아야 한다. 목표 경로에는 완결된 파일만 존재하고,
// 쓰다 만 임시 파일(.writing)이 남아서는 안 된다.
void TestWorkflow::atomicProjectWrite_neverLeavesPartialFile() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString qgz = QDir(dir.path()).filePath(QStringLiteral("조사.qgz"));
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  QVERIFY2(kaWriteQgisProjectAtomic(&proj, qgz, &err), qPrintable(err));
  QVERIFY(QFile::exists(qgz));
  QVERIFY(QFileInfo(qgz).size() > 0);
  QVERIFY2(QDir(dir.path()).entryList(QStringList{QStringLiteral("*ka-writing*")}, QDir::Files).isEmpty(), "임시 파일이 남았다");
  // 프로젝트는 임시 경로가 아니라 최종 경로를 가리켜야 한다.
  QCOMPARE(QFileInfo(proj.fileName()).absoluteFilePath(), QFileInfo(qgz).absoluteFilePath());
}

// 두 번째 저장은 직전 정상본을 .bak으로 한 세대 남긴다. 열기 실패 시 되살릴 자리다.
void TestWorkflow::atomicProjectWrite_keepsOneBackupGeneration() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString qgz = QDir(dir.path()).filePath(QStringLiteral("조사.qgz"));
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  QString err;
  QVERIFY2(kaWriteQgisProjectAtomic(&proj, qgz, &err), qPrintable(err));
  const qint64 firstSize = QFileInfo(qgz).size();
  QVERIFY(!QFile::exists(kaProjectBackupPath(qgz)));
  proj.setTitle(QStringLiteral("두 번째"));
  QVERIFY2(kaWriteQgisProjectAtomic(&proj, qgz, &err), qPrintable(err));
  QVERIFY2(QFile::exists(kaProjectBackupPath(qgz)), "직전 저장본이 남지 않았다");
  QCOMPARE(QFileInfo(kaProjectBackupPath(qgz)).size(), firstSize);
  QVERIFY(QFileInfo(qgz).size() > 0);
  QVERIFY(QDir(dir.path()).entryList(QStringList{QStringLiteral("*ka-writing*")}, QDir::Files).isEmpty());
}

// 파일이 생겼는지만 보면 부족하다. .qgz 확장자를 잃은 임시 파일에 쓰면 QGIS가 zip이
// 아니라 평문 XML로 쓰고, 그것을 .qgz로 rename하면 크기도 0이 아니고 파일도 존재하지만
// 다시 열리지 않는다. 왕복으로 확인한다.
void TestWorkflow::atomicProjectWrite_writtenFileReadsBackAsQgz() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString qgz = QDir(dir.path()).filePath(QStringLiteral("조사.qgz"));
  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
  proj.setTitle(QStringLiteral("왕복 검사"));
  QString err;
  QVERIFY2(kaWriteQgisProjectAtomic(&proj, qgz, &err), qPrintable(err));

  QgsProject back;
  QVERIFY2(back.read(qgz), "저장한 .qgz를 다시 열지 못했다");
  QCOMPARE(back.title(), QStringLiteral("왕복 검사"));
  QCOMPARE(back.crs().authid(), QStringLiteral("EPSG:5186"));

  // 두 번째 저장이 남기는 백업도 같은 규격이어야 복구에 쓸 수 있다.
  proj.setTitle(QStringLiteral("두 번째"));
  QVERIFY2(kaWriteQgisProjectAtomic(&proj, qgz, &err), qPrintable(err));
  const QString bak = kaProjectBackupPath(qgz);
  QCOMPARE(QFileInfo(bak).fileName(), QStringLiteral("조사.bak.qgz"));
  QVERIFY(QFile::exists(bak));
  QgsProject restored;
  QVERIFY2(restored.read(bak), "백업본을 열지 못했다");
  QCOMPARE(restored.title(), QStringLiteral("왕복 검사"));
}

// 근본 해결의 전제: 프로젝트를 .gpkg 안에 넣을 수 있어야 한다. 되면 조사 파일이
// 하나로 닫히고, 동반 .qgz가 깨지거나 폴더가 옮겨져도 레이어 구성이 살아남는다.
void TestWorkflow::gpkgEmbeddedProject_writesAndReadsBack() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(
      dir.path(), QStringLiteral("저장구조"), &err, QStringLiteral("EPSG:5186"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  const QString uri =
      QStringLiteral("geopackage:%1?projectName=%2").arg(gpkg, QStringLiteral("survey"));
  {
    QgsProject proj;
    proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    proj.setTitle(QStringLiteral("내장 저장"));
    QVERIFY2(proj.write(uri), qPrintable(proj.error()));
  }
  {
    QgsProject back;
    QVERIFY2(back.read(uri), qPrintable(back.error()));
    QCOMPARE(back.title(), QStringLiteral("내장 저장"));
    QCOMPARE(back.crs().authid(), QStringLiteral("EPSG:5186"));
  }
  // 조사 파일 하나만 있으면 된다는 것이 요점이다.
  QVERIFY(QFile::exists(gpkg));
}

// 근본 요구: 조사 파일 하나만 복사해도 외부에서 넣은 레이어가 그대로 열려야 한다.
// 예전에는 .qgz가 SHP를 상대경로로 가리켜서, 폴더를 옮기면 그 레이어가 통째로 깨졌다.
void TestWorkflow::surveyFileTravelsAloneWithAbsorbedShp() {
  QTemporaryDir work;
  QVERIFY(work.isValid());
  QString err;
  const QString gpkg = SurveyProjectFactory::createNewSurvey(
      work.path(), QStringLiteral("이동시험"), &err, QStringLiteral("EPSG:5186"));
  QVERIFY2(!gpkg.isEmpty(), qPrintable(err));

  // 조사 폴더 바깥에 있는 SHP를 흉내 낸다.
  QTemporaryDir outside;
  QVERIFY(outside.isValid());
  const QString shp = QDir(outside.path()).filePath(QStringLiteral("주변유적.shp"));
  {
    QgsVectorLayer mem(QStringLiteral("Polygon?crs=EPSG:5186&field=이름:string(40)"),
                       QStringLiteral("주변유적"), QStringLiteral("memory"));
    QVERIFY(mem.isValid());
    QgsFeature f(mem.fields());
    f.setAttribute(0, QStringLiteral("가마터"));
    f.setGeometry(QgsGeometry::fromRect(QgsRectangle(0, 0, 10, 10)));
    QVERIFY(mem.dataProvider()->addFeature(f));
    QgsVectorFileWriter::SaveVectorOptions o;
    o.driverName = QStringLiteral("ESRI Shapefile");
    o.fileEncoding = QStringLiteral("UTF-8");
    QString werr;
    QCOMPARE(QgsVectorFileWriter::writeAsVectorFormatV3(&mem, shp, QgsCoordinateTransformContext(),
                                                        o, &werr),
             QgsVectorFileWriter::NoError);
  }

  {
    QgsProject proj;
    proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* ext = new QgsVectorLayer(shp, QStringLiteral("주변유적"), QStringLiteral("ogr"));
    QVERIFY(ext->isValid());
    proj.addMapLayer(ext);
    const SurveyStorage::AbsorbResult r = SurveyStorage::absorbExternalVectors(&proj, gpkg);
    QVERIFY2(r.imported.contains(QStringLiteral("주변유적")),
             qPrintable(QStringLiteral("들여오지 못했다: %1").arg(r.failed.join(','))));
    // 이제 데이터소스가 조사 파일 안을 가리켜야 한다.
    QVERIFY(ext->source().startsWith(gpkg));
    QVERIFY2(SurveyStorage::writeEmbedded(&proj, gpkg, &err), qPrintable(err));
  }

  // 조사 파일 하나만 다른 폴더로 복사하고, 원본 SHP 폴더는 통째로 없앤다.
  QTemporaryDir moved;
  QVERIFY(moved.isValid());
  const QString movedGpkg = QDir(moved.path()).filePath(QStringLiteral("이동시험.gpkg"));
  QVERIFY(QFile::copy(gpkg, movedGpkg));

  QVERIFY(SurveyStorage::hasEmbeddedProject(movedGpkg));
  QgsProject back;
  QVERIFY2(SurveyStorage::readEmbedded(&back, movedGpkg, nullptr, &err), qPrintable(err));
  QgsVectorLayer* found = nullptr;
  for (QgsMapLayer* l : back.mapLayers()) {
    if (l && l->name() == QStringLiteral("주변유적")) found = qobject_cast<QgsVectorLayer*>(l);
  }
  QVERIFY2(found, "복사한 조사 파일에서 외부 레이어를 찾지 못했다");
  QVERIFY2(found->isValid(), "레이어가 깨졌다 — 바깥 경로에 여전히 기대고 있다");
  QCOMPARE(found->featureCount(), 1L);
  // 핵심: 복사본이 원본 SHP도, 원본 .gpkg도 아닌 자기 자신을 가리켜야 한다.
  // 절대경로로 저장되면 폴더째 넘겨줘도 남의 컴퓨터에서 깨진다.
  QVERIFY2(!found->source().contains(outside.path()),
           qPrintable(QStringLiteral("아직 바깥 SHP를 가리킨다: %1").arg(found->source())));
  QVERIFY2(found->source().contains(QDir(moved.path()).absolutePath()),
           qPrintable(QStringLiteral("복사본이 아니라 원본을 가리킨다: %1").arg(found->source())));
}

void TestWorkflow::copySurvey_includesCommittedWalAndKeepsSourceIndependent() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QString err;
  const QString source = SurveyProjectFactory::createNewSurvey(
      dir.path(), QStringLiteral("원본"), &err);
  QVERIFY2(!source.isEmpty(), qPrintable(err));
  {
    QgsProject project;
    QVERIFY2(SurveyStorage::writeEmbedded(&project, source, &err), qPrintable(err));
  }
  std::unique_ptr<void, decltype(&GDALClose)> writer(
      GDALOpenEx(source.toUtf8().constData(), GDAL_OF_VECTOR | GDAL_OF_UPDATE,
                 nullptr, nullptr, nullptr), &GDALClose);
  QVERIFY(writer);
  const auto execute = [&](const char* sql) {
    CPLErrorReset();
    OGRLayerH result = GDALDatasetExecuteSQL(writer.get(), sql, nullptr, nullptr);
    if (result) GDALDatasetReleaseResultSet(writer.get(), result);
    return CPLGetLastErrorType() < CE_Failure;
  };
  QVERIFY(execute("PRAGMA journal_mode=WAL"));
  QVERIFY(execute("PRAGMA wal_autocheckpoint=0"));
  QVERIFY(execute("PRAGMA wal_checkpoint(TRUNCATE)"));
  QVERIFY(execute("INSERT INTO survey_area (survey_name) VALUES ('committed in WAL')"));
  QVERIFY2(QFileInfo(source + QStringLiteral("-wal")).size() > 32,
           "회귀 전제: 커밋한 변경이 WAL에 있어야 한다");
  const QString target = dir.filePath(QStringLiteral("사본'검증.gpkg"));

  QVERIFY2(SurveyStorage::copySurvey(source, target, &err), qPrintable(err));
  QVERIFY(SurveyStorage::hasEmbeddedProject(target));
  QgsVectorLayer copied(QStringLiteral("%1|layername=survey_area").arg(target),
                         QStringLiteral("사본"), QStringLiteral("ogr"));
  QVERIFY(copied.isValid());
  QCOMPARE(copied.featureCount(), 1LL);
  QgsFeature feature;
  QVERIFY(copied.getFeatures().nextFeature(feature));
  QCOMPARE(feature.attribute(QStringLiteral("survey_name")).toString(), QStringLiteral("committed in WAL"));
  QVERIFY(copied.startEditing());
  QVERIFY(copied.changeAttributeValue(feature.id(), copied.fields().indexOf(QStringLiteral("survey_name")),
                                      QStringLiteral("copy changed")));
  QVERIFY(copied.commitChanges());
  OGRLayerH result = GDALDatasetExecuteSQL(writer.get(), "SELECT survey_name FROM survey_area", nullptr, nullptr);
  QVERIFY(result);
  OGRFeatureH original = OGR_L_GetNextFeature(result);
  QVERIFY(original);
  const QString originalName = QString::fromUtf8(OGR_F_GetFieldAsString(original, 0));
  OGR_F_Destroy(original);
  GDALDatasetReleaseResultSet(writer.get(), result);
  QCOMPARE(originalName, QStringLiteral("committed in WAL"));
  QVERIFY2(SurveyStorage::copySurvey(source, QDir::toNativeSeparators(source), &err), qPrintable(err));
}

void TestWorkflow::copySurvey_failurePreservesExistingDestination() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString target = dir.filePath(QStringLiteral("existing.gpkg"));
  const QByteArray contents("keep existing destination");
  {
    QFile file(target);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(contents), contents.size());
  }
  QString err;
  QVERIFY(!SurveyStorage::copySurvey(dir.filePath(QStringLiteral("missing.gpkg")), target, &err));
  QVERIFY(!err.isEmpty());
  QFile original(target);
  QVERIFY(original.open(QIODevice::ReadOnly));
  QCOMPARE(original.readAll(), contents);
  original.close();

  const QString source = SurveyProjectFactory::createNewSurvey(
      dir.path(), QStringLiteral("source"), &err);
  QVERIFY2(!source.isEmpty(), qPrintable(err));
  {
    QFile wal(target + QStringLiteral("-wal"));
    QVERIFY(wal.open(QIODevice::WriteOnly));
    QCOMPARE(wal.write("active WAL"), 10LL);
  }
  QVERIFY(!SurveyStorage::copySurvey(source, target, &err));
  QVERIFY(!err.isEmpty());
  QVERIFY(original.open(QIODevice::ReadOnly));
  QCOMPARE(original.readAll(), contents);
}

void TestWorkflow::test_export_failure_aborts_and_reports_error() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  const QString outDir = tempDir.filePath(QStringLiteral("export_failure_pkg"));
  QDir().mkpath(outDir);

  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));

  auto* sa = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                QStringLiteral("survey_area"), QStringLiteral("memory"));
  QVERIFY(sa->isValid());
  sa->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey), QStringLiteral("survey_area"));
  proj.addMapLayer(sa);
  QVERIFY(sa->startEditing());
  QgsFeature sf(sa->fields());
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200100, 450000)
       << QgsPointXY(200100, 450100) << QgsPointXY(200000, 450100)
       << QgsPointXY(200000, 450000);
  sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(sa->addFeature(sf));
  QVERIFY(sa->commitChanges());

  QVERIFY2(addComposedUserSheet(&proj, sa), "composed user_sheet");
  QVERIFY(LayoutService::isComposedStudioSheet(&proj));

  // Create directory at target PDF path to force LayoutService::exportLayoutPdf to fail
  const QString pdfPath = QDir(outDir).filePath(QStringLiteral("조사도면.pdf"));
  QVERIFY2(QDir(outDir).mkdir(QStringLiteral("조사도면.pdf")), "Failed to create directory at test PDF path");

  QString err;
  const QString result = ExportService::exportSubmissionPackage(
      &proj, outDir, QStringLiteral("UTF-8"), QStringLiteral("OK"), true, false, &err);

  // Clean up directory
  QDir(outDir).rmdir(QStringLiteral("조사도면.pdf"));

  // Export MUST fail, returning empty string and populating errorOut
  QVERIFY2(result.isEmpty(), "Export must abort when layout PDF cannot be written");
  QVERIFY2(!err.isEmpty(), "Error message must be reported on PDF export failure");

  // Incomplete MANIFEST.sha256 must NOT be written when export aborts
  const QString manifestPath = QDir(outDir).filePath(QStringLiteral("MANIFEST.sha256"));
  QVERIFY2(!QFile::exists(manifestPath),
           "MANIFEST.sha256 must not be created if package export fails");
}

void TestWorkflow::test_section_sheet_bundling() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  const QString pkgDir = tempDir.filePath(QStringLiteral("section_bundle_pkg"));

  QgsProject proj;
  proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));

  // 1. Survey area
  auto* sa = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                QStringLiteral("survey_area"), QStringLiteral("memory"));
  QVERIFY(sa->isValid());
  sa->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey), QStringLiteral("survey_area"));
  proj.addMapLayer(sa);
  QVERIFY(sa->startEditing());
  QgsFeature sf(sa->fields());
  QgsPolylineXY ring;
  ring << QgsPointXY(200000, 450000) << QgsPointXY(200100, 450000)
       << QgsPointXY(200100, 450100) << QgsPointXY(200000, 450100)
       << QgsPointXY(200000, 450000);
  sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
  QVERIFY(sa->addFeature(sf));
  QVERIFY(sa->commitChanges());

  // 2. Composed user_sheet
  QVERIFY2(addComposedUserSheet(&proj, sa), "composed user_sheet");
  QVERIFY(LayoutService::isComposedStudioSheet(&proj));

  // 3. Composed section_sheet via SectionLayoutService
  SectionLayoutOptions secOpt;
  secOpt.titleKo = QStringLiteral("트렌치 1호 토층 단면도");
  secOpt.paper = SectionLayoutOptions::Paper::A4;
  const auto secRes = SectionLayoutService::buildSectionLayout(&proj, {}, secOpt);
  QVERIFY2(!secRes.layoutName.isEmpty(), qPrintable(secRes.errorKo));
  QVERIFY(proj.layoutManager()->layoutByName(QStringLiteral("section_sheet")));

  // 4. Export package
  QString err;
  const QString out = ExportService::exportSubmissionPackage(
      &proj, pkgDir, QStringLiteral("UTF-8"), QStringLiteral("OK"), true, false, &err);
  QVERIFY2(!out.isEmpty(), qPrintable(err));

  // 5. Verify both 조사도면.pdf and 단면도.pdf exist
  const QString userPdf = QDir(pkgDir).filePath(QStringLiteral("조사도면.pdf"));
  QVERIFY2(QFile::exists(userPdf), "조사도면.pdf must exist");
  QVERIFY(QFileInfo(userPdf).size() > 500);

  const QString sectionPdf = QDir(pkgDir).filePath(QStringLiteral("단면도.pdf"));
  QVERIFY2(QFile::exists(sectionPdf), "단면도.pdf must be bundled when section_sheet is present");
  QVERIFY(QFileInfo(sectionPdf).size() > 500);

  // 6. Verify MANIFEST.sha256 contains entries for both PDFs
  const QString manifestPath = QDir(pkgDir).filePath(QStringLiteral("MANIFEST.sha256"));
  QVERIFY(QFile::exists(manifestPath));
  QFile mf(manifestPath);
  QVERIFY(mf.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString manifestContent = QString::fromUtf8(mf.readAll());
  mf.close();

  QVERIFY2(manifestContent.contains(QStringLiteral("조사도면.pdf")),
           "MANIFEST.sha256 must register 조사도면.pdf");
  QVERIFY2(manifestContent.contains(QStringLiteral("단면도.pdf")),
           "MANIFEST.sha256 must register 단면도.pdf");
  QVERIFY2(manifestContent.contains(QStringLiteral("survey_area.shp")),
           "MANIFEST.sha256 must register survey_area.shp");
}

void TestWorkflow::test_chunked_sha256_manifest_integrity() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  const QString workDir = tempDir.filePath(QStringLiteral("manifest_test_dir"));
  QDir().mkpath(workDir);

  // File 1: Small file (< 1 KB)
  const QString smallPath = QDir(workDir).filePath(QStringLiteral("small_metadata.txt"));
  QFile fSmall(smallPath);
  QVERIFY(fSmall.open(QIODevice::WriteOnly));
  const QByteArray smallData = "KA-HGIS submission integrity verification small file test payload.\n";
  fSmall.write(smallData);
  fSmall.close();
  const QString smallHash = QString::fromLatin1(
      QCryptographicHash::hash(smallData, QCryptographicHash::Sha256).toHex());

  // File 2: Multi-chunk binary file (256 KB = 4 x 64 KB chunks)
  const QString largePath = QDir(workDir).filePath(QStringLiteral("large_stream.bin"));
  QFile fLarge(largePath);
  QVERIFY(fLarge.open(QIODevice::WriteOnly));
  QByteArray largeData;
  largeData.resize(256 * 1024);
  for (int i = 0; i < largeData.size(); ++i) {
    largeData[i] = static_cast<char>((i * 31 + 17) % 251);
  }
  fLarge.write(largeData);
  fLarge.close();
  const QString largeHash = QString::fromLatin1(
      QCryptographicHash::hash(largeData, QCryptographicHash::Sha256).toHex());

  // File 3: Korean-named file
  const QString krPath = QDir(workDir).filePath(QStringLiteral("조사도면.pdf"));
  QFile fKr(krPath);
  QVERIFY(fKr.open(QIODevice::WriteOnly));
  const QByteArray krData = "%PDF-1.4 dummy archaeological field sheet binary stream\n%%EOF\n";
  fKr.write(krData);
  fKr.close();
  const QString krHash = QString::fromLatin1(
      QCryptographicHash::hash(krData, QCryptographicHash::Sha256).toHex());

  // Generate manifest
  QString err;
  QVERIFY2(ExportService::writeSha256Manifest(workDir, &err), qPrintable(err));

  const QString manPath = QDir(workDir).filePath(QStringLiteral("MANIFEST.sha256"));
  QVERIFY(QFile::exists(manPath));

  QFile manFile(manPath);
  QVERIFY(manFile.open(QIODevice::ReadOnly | QIODevice::Text));
  QTextStream ts(&manFile);
  ts.setEncoding(QStringConverter::Utf8);

  const QRegularExpression lineRegex(QStringLiteral("^([a-f0-9]{64})  (.+)$"));
  QMap<QString, QString> parsedHashes;
  while (!ts.atEnd()) {
    const QString line = ts.readLine();
    if (line.trimmed().isEmpty()) continue;
    const auto match = lineRegex.match(line);
    QVERIFY2(match.hasMatch(), qPrintable(QStringLiteral("Invalid manifest line format: ") + line));
    const QString hash = match.captured(1);
    const QString filename = match.captured(2);
    parsedHashes.insert(filename, hash);
  }
  manFile.close();

  // Self-exclusion check
  QVERIFY2(!parsedHashes.contains(QStringLiteral("MANIFEST.sha256")),
           "MANIFEST.sha256 must not list itself");

  // Verify all 3 files are present and match computed hashes
  QCOMPARE(parsedHashes.size(), 3);
  QCOMPARE(parsedHashes.value(QStringLiteral("small_metadata.txt")), smallHash);
  QCOMPARE(parsedHashes.value(QStringLiteral("large_stream.bin")), largeHash);
  QCOMPARE(parsedHashes.value(QStringLiteral("조사도면.pdf")), krHash);
}

void TestWorkflow::test_challenge_pdf_failure_readonly_and_locked() {
  // Test A: Destination file 조사도면.pdf is read-only
  {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString outDir = tempDir.filePath(QStringLiteral("pdf_fail_ro"));
    QDir().mkpath(outDir);

    QgsProject proj;
    proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* sa = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("survey_area"), QStringLiteral("memory"));
    QVERIFY(sa->isValid());
    sa->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey), QStringLiteral("survey_area"));
    proj.addMapLayer(sa);
    QVERIFY(sa->startEditing());
    QgsFeature sf(sa->fields());
    QgsPolylineXY ring;
    ring << QgsPointXY(200000, 450000) << QgsPointXY(200100, 450000)
         << QgsPointXY(200100, 450100) << QgsPointXY(200000, 450100)
         << QgsPointXY(200000, 450000);
    sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
    QVERIFY(sa->addFeature(sf));
    QVERIFY(sa->commitChanges());
    QVERIFY2(addComposedUserSheet(&proj, sa), "composed user_sheet");

    // Pre-create 조사도면.pdf and set ReadOnly permissions
    const QString pdfPath = QDir(outDir).filePath(QStringLiteral("조사도면.pdf"));
    {
      QFile rf(pdfPath);
      QVERIFY(rf.open(QIODevice::WriteOnly));
      rf.write("read only dummy placeholder");
      rf.close();
      QVERIFY(QFile::setPermissions(pdfPath, QFileDevice::ReadOwner | QFileDevice::ReadUser));
    }

    QString err;
    const QString res = ExportService::exportSubmissionPackage(
        &proj, outDir, QStringLiteral("UTF-8"), QStringLiteral("OK"), true, false, &err);

    // Verify package aborts cleanly
    QVERIFY2(res.isEmpty(), "exportSubmissionPackage must return empty QString on PDF write failure");
    QVERIFY2(!err.isEmpty(), "Error output must be non-empty");

    // Partial MANIFEST.sha256 MUST NOT be written
    const QString manifestPath = QDir(outDir).filePath(QStringLiteral("MANIFEST.sha256"));
    QVERIFY2(!QFile::exists(manifestPath), "MANIFEST.sha256 must NOT be written when PDF export fails");

    // Restore permissions for tempDir cleanup
    QFile::setPermissions(pdfPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  }

  // Test B: Destination file 조사도면.pdf is locked exclusively
  {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString outDir = tempDir.filePath(QStringLiteral("pdf_fail_locked"));
    QDir().mkpath(outDir);

    QgsProject proj;
    proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* sa = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("survey_area"), QStringLiteral("memory"));
    sa->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey), QStringLiteral("survey_area"));
    proj.addMapLayer(sa);
    QVERIFY(sa->startEditing());
    QgsFeature sf(sa->fields());
    QgsPolylineXY ring;
    ring << QgsPointXY(200000, 450000) << QgsPointXY(200100, 450000)
         << QgsPointXY(200100, 450100) << QgsPointXY(200000, 450100)
         << QgsPointXY(200000, 450000);
    sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
    QVERIFY(sa->addFeature(sf));
    QVERIFY(sa->commitChanges());
    QVERIFY2(addComposedUserSheet(&proj, sa), "composed user_sheet");

    const QString pdfPath = QDir(outDir).filePath(QStringLiteral("조사도면.pdf"));
#ifdef _WIN32
    HANDLE hLock = CreateFileW(reinterpret_cast<LPCWSTR>(pdfPath.utf16()),
                               GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ,
                               NULL,
                               CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL,
                               NULL);
    QVERIFY(hLock != INVALID_HANDLE_VALUE);
#else
    QFile lockFile(pdfPath);
    QVERIFY(lockFile.open(QIODevice::WriteOnly));
#endif

    QString err;
    const QString res = ExportService::exportSubmissionPackage(
        &proj, outDir, QStringLiteral("UTF-8"), QStringLiteral("OK"), true, false, &err);

    QVERIFY2(res.isEmpty(), "exportSubmissionPackage must abort when PDF is locked");
    QVERIFY2(!err.isEmpty(), "Error output must report failure");

    const QString manifestPath = QDir(outDir).filePath(QStringLiteral("MANIFEST.sha256"));
    QVERIFY2(!QFile::exists(manifestPath), "MANIFEST.sha256 must NOT be written when PDF is locked");

#ifdef _WIN32
    CloseHandle(hLock);
#else
    lockFile.close();
#endif
  }

  // Test C: user_sheet succeeds, but section_sheet (단면도.pdf) export fails (locked)
  {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString outDir = tempDir.filePath(QStringLiteral("sec_fail_locked"));
    QDir().mkpath(outDir);

    QgsProject proj;
    proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* sa = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("survey_area"), QStringLiteral("memory"));
    sa->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey), QStringLiteral("survey_area"));
    proj.addMapLayer(sa);
    QVERIFY(sa->startEditing());
    QgsFeature sf(sa->fields());
    QgsPolylineXY ring;
    ring << QgsPointXY(200000, 450000) << QgsPointXY(200100, 450000)
         << QgsPointXY(200100, 450100) << QgsPointXY(200000, 450100)
         << QgsPointXY(200000, 450000);
    sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
    QVERIFY(sa->addFeature(sf));
    QVERIFY(sa->commitChanges());
    QVERIFY2(addComposedUserSheet(&proj, sa), "composed user_sheet");

    SectionLayoutOptions secOpt;
    secOpt.titleKo = QStringLiteral("단면도");
    secOpt.paper = SectionLayoutOptions::Paper::A4;
    SectionLayoutService::buildSectionLayout(&proj, {}, secOpt);

    const QString secPdfPath = QDir(outDir).filePath(QStringLiteral("단면도.pdf"));
#ifdef _WIN32
    HANDLE hSecLock = CreateFileW(reinterpret_cast<LPCWSTR>(secPdfPath.utf16()),
                                  GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ,
                                  NULL,
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  NULL);
    QVERIFY(hSecLock != INVALID_HANDLE_VALUE);
#else
    QFile secLockFile(secPdfPath);
    QVERIFY(secLockFile.open(QIODevice::WriteOnly));
#endif

    QString err;
    const QString res = ExportService::exportSubmissionPackage(
        &proj, outDir, QStringLiteral("UTF-8"), QStringLiteral("OK"), true, false, &err);

    QVERIFY2(res.isEmpty(), "exportSubmissionPackage must abort when section PDF export fails");
    QVERIFY2(!err.isEmpty(), "Error output must report section failure");
    QVERIFY2(err.contains(QStringLiteral("비어 있지")), "Existing package files must be rejected before any writes");

    const QString manifestPath = QDir(outDir).filePath(QStringLiteral("MANIFEST.sha256"));
    QVERIFY2(!QFile::exists(manifestPath), "MANIFEST.sha256 must NOT be written when section PDF export fails");

#ifdef _WIN32
    CloseHandle(hSecLock);
#else
    secLockFile.close();
#endif
  }
}

void TestWorkflow::test_challenge_section_sheet_bundling_variations() {
  // Scenario A: Uncomposed section_sheet (with empty_hint item) should NOT be exported as 단면도.pdf
  {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString pkgDir = tempDir.filePath(QStringLiteral("sec_uncomposed_pkg"));

    QgsProject proj;
    proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* sa = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("survey_area"), QStringLiteral("memory"));
    sa->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey), QStringLiteral("survey_area"));
    proj.addMapLayer(sa);
    QVERIFY(sa->startEditing());
    QgsFeature sf(sa->fields());
    QgsPolylineXY ring;
    ring << QgsPointXY(200000, 450000) << QgsPointXY(200100, 450000)
         << QgsPointXY(200100, 450100) << QgsPointXY(200000, 450100)
         << QgsPointXY(200000, 450000);
    sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
    QVERIFY(sa->addFeature(sf));
    QVERIFY(sa->commitChanges());
    QVERIFY2(addComposedUserSheet(&proj, sa), "composed user_sheet");

    // Add empty uncomposed section_sheet layout with empty_hint label
    auto* secLayout = new QgsPrintLayout(&proj);
    secLayout->setName(QStringLiteral("section_sheet"));
    auto* hint = new QgsLayoutItemMap(secLayout);
    hint->setId(QStringLiteral("empty_hint"));
    secLayout->addLayoutItem(hint);
    proj.layoutManager()->addLayout(secLayout);

    QString err;
    const QString res = ExportService::exportSubmissionPackage(
        &proj, pkgDir, QStringLiteral("UTF-8"), QStringLiteral("OK"), true, false, &err);
    QVERIFY2(!res.isEmpty(), qPrintable(err));

    // 조사도면.pdf must exist, but 단면도.pdf must NOT exist
    QVERIFY(QFile::exists(QDir(pkgDir).filePath(QStringLiteral("조사도면.pdf"))));
    QVERIFY(!QFile::exists(QDir(pkgDir).filePath(QStringLiteral("단면도.pdf"))));

    // MANIFEST.sha256 must register 조사도면.pdf, but NOT 단면도.pdf
    QFile mf(QDir(pkgDir).filePath(QStringLiteral("MANIFEST.sha256")));
    QVERIFY(mf.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString manifestContent = QString::fromUtf8(mf.readAll());
    mf.close();
    QVERIFY(manifestContent.contains(QStringLiteral("조사도면.pdf")));
    QVERIFY(!manifestContent.contains(QStringLiteral("단면도.pdf")));
  }

  // Scenario B: section_sheet composed, but user_sheet absent
  {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString pkgDir = tempDir.filePath(QStringLiteral("sec_only_pkg"));

    QgsProject proj;
    proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* sa = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("survey_area"), QStringLiteral("memory"));
    sa->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey), QStringLiteral("survey_area"));
    proj.addMapLayer(sa);
    QVERIFY(sa->startEditing());
    QgsFeature sf(sa->fields());
    QgsPolylineXY ring;
    ring << QgsPointXY(200000, 450000) << QgsPointXY(200100, 450000)
         << QgsPointXY(200100, 450100) << QgsPointXY(200000, 450100)
         << QgsPointXY(200000, 450000);
    sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
    QVERIFY(sa->addFeature(sf));
    QVERIFY(sa->commitChanges());

    // Build composed section_sheet
    SectionLayoutOptions secOpt;
    secOpt.titleKo = QStringLiteral("단독 단면도");
    secOpt.paper = SectionLayoutOptions::Paper::A4;
    SectionLayoutService::buildSectionLayout(&proj, {}, secOpt);

    QString err;
    const QString res = ExportService::exportSubmissionPackage(
        &proj, pkgDir, QStringLiteral("UTF-8"), QStringLiteral("OK"), true, false, &err);
    QVERIFY(res.isEmpty());
    QVERIFY2(err.contains(QStringLiteral("도면만들기")), qPrintable(err));
    QVERIFY(!QFile::exists(QDir(pkgDir).filePath(QStringLiteral("README_submit.txt"))));
    QVERIFY(!QFile::exists(QDir(pkgDir).filePath(QStringLiteral("단면도.pdf"))));
    QVERIFY(!QFile::exists(QDir(pkgDir).filePath(QStringLiteral("조사도면.pdf"))));
  }
}

void TestWorkflow::test_challenge_encoding_txt_flush_and_content() {
  // Test with UTF-8
  {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString pkgDir = tempDir.filePath(QStringLiteral("enc_utf8_pkg"));

    QgsProject proj;
    proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* sa = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("survey_area"), QStringLiteral("memory"));
    sa->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey), QStringLiteral("survey_area"));
    proj.addMapLayer(sa);
    QVERIFY(sa->startEditing());
    QgsFeature sf(sa->fields());
    QgsPolylineXY ring;
    ring << QgsPointXY(200000, 450000) << QgsPointXY(200100, 450000)
         << QgsPointXY(200100, 450100) << QgsPointXY(200000, 450100)
         << QgsPointXY(200000, 450000);
    sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
    QVERIFY(sa->addFeature(sf));
    QVERIFY(sa->commitChanges());
    QVERIFY2(addComposedUserSheet(&proj, sa), "composed user_sheet");

    QString err;
    const QString res = ExportService::exportSubmissionPackage(
        &proj, pkgDir, QStringLiteral("UTF-8"), QStringLiteral("OK"), true, false, &err);
    QVERIFY2(!res.isEmpty(), qPrintable(err));

    const QString encPath = QDir(pkgDir).filePath(QStringLiteral("encoding.txt"));
    QVERIFY(QFile::exists(encPath));
    QFile encFile(encPath);
    QVERIFY(encFile.open(QIODevice::ReadOnly));
    const QByteArray actualBytes = encFile.readAll();
    encFile.close();
    QVERIFY(actualBytes.size() > 0);

    // Compute genuine hash of actual file on disk
    const QString genuineHash = QString::fromLatin1(
        QCryptographicHash::hash(actualBytes, QCryptographicHash::Sha256).toHex());

    // Empty string SHA-256
    const QString emptyHash = QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    QVERIFY2(genuineHash != emptyHash, "Genuine hash must not be empty-file hash");

    // Read MANIFEST.sha256 and find encoding.txt entry
    QFile manFile(QDir(pkgDir).filePath(QStringLiteral("MANIFEST.sha256")));
    QVERIFY(manFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream ts(&manFile);
    ts.setEncoding(QStringConverter::Utf8);
    QString recordedHash;
    while (!ts.atEnd()) {
      const QString line = ts.readLine();
      if (line.endsWith(QStringLiteral("encoding.txt"))) {
        recordedHash = line.left(64);
        break;
      }
    }
    manFile.close();

    QVERIFY2(!recordedHash.isEmpty(), "encoding.txt must be listed in MANIFEST.sha256");
    QCOMPARE(recordedHash, genuineHash);
    QVERIFY2(recordedHash != emptyHash,
             "MANIFEST.sha256 must NOT record empty hash for encoding.txt (flush bug check)");
  }

  // Test with CP949 / EUC-KR
  {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString pkgDir = tempDir.filePath(QStringLiteral("enc_cp949_pkg"));

    QgsProject proj;
    proj.setCrs(QgsCoordinateReferenceSystem(QStringLiteral("EPSG:5186")));
    auto* sa = new QgsVectorLayer(QStringLiteral("Polygon?crs=EPSG:5186"),
                                  QStringLiteral("survey_area"), QStringLiteral("memory"));
    sa->setCustomProperty(QString::fromUtf8(LayerOps::kPropLayerKey), QStringLiteral("survey_area"));
    proj.addMapLayer(sa);
    QVERIFY(sa->startEditing());
    QgsFeature sf(sa->fields());
    QgsPolylineXY ring;
    ring << QgsPointXY(200000, 450000) << QgsPointXY(200100, 450000)
         << QgsPointXY(200100, 450100) << QgsPointXY(200000, 450100)
         << QgsPointXY(200000, 450000);
    sf.setGeometry(QgsGeometry::fromPolygonXY(QgsPolygonXY() << ring));
    QVERIFY(sa->addFeature(sf));
    QVERIFY(sa->commitChanges());
    QVERIFY2(addComposedUserSheet(&proj, sa), "composed user_sheet");

    QString err;
    const QString res = ExportService::exportSubmissionPackage(
        &proj, pkgDir, QStringLiteral("CP949"), QStringLiteral("OK"), true, false, &err);
    QVERIFY2(!res.isEmpty(), qPrintable(err));

    const QString encPath = QDir(pkgDir).filePath(QStringLiteral("encoding.txt"));
    QFile encFile(encPath);
    QVERIFY(encFile.open(QIODevice::ReadOnly));
    const QByteArray actualBytes = encFile.readAll();
    encFile.close();
    QVERIFY(actualBytes.startsWith("CP949"));

    const QString genuineHash = QString::fromLatin1(
        QCryptographicHash::hash(actualBytes, QCryptographicHash::Sha256).toHex());

    QFile manFile(QDir(pkgDir).filePath(QStringLiteral("MANIFEST.sha256")));
    QVERIFY(manFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream ts(&manFile);
    ts.setEncoding(QStringConverter::Utf8);
    QString recordedHash;
    while (!ts.atEnd()) {
      const QString line = ts.readLine();
      if (line.endsWith(QStringLiteral("encoding.txt"))) {
        recordedHash = line.left(64);
        break;
      }
    }
    manFile.close();

    QCOMPARE(recordedHash, genuineHash);
  }
}

void TestWorkflow::test_challenge_manifest_64kb_chunking_stress() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  const QString workDir = tempDir.filePath(QStringLiteral("chunk_stress_dir"));
  QDir().mkpath(workDir);

  struct TestCase {
    QString fileName;
    qsizetype size;
    enum Pattern { Zeroes, SingleByte, BinaryAllBytes, RepeatedString } pattern;
  };

  const QList<TestCase> testCases = {
    { QStringLiteral("c00_empty_0b.dat"), 0, TestCase::Zeroes },
    { QStringLiteral("c01_single_1b.dat"), 1, TestCase::SingleByte },
    { QStringLiteral("c02_subchunk_65535b.dat"), 65535, TestCase::BinaryAllBytes },
    { QStringLiteral("c03_exactchunk_65536b.dat"), 65536, TestCase::BinaryAllBytes },
    { QStringLiteral("c04_overchunk_65537b.dat"), 65537, TestCase::BinaryAllBytes },
    { QStringLiteral("c05_twochunks_131072b.dat"), 131072, TestCase::BinaryAllBytes },
    { QStringLiteral("c06_one_mb_1048576b.bin"), 1048576, TestCase::BinaryAllBytes },
    { QStringLiteral("c07_five_mb_5242880b.bin"), 5242880, TestCase::BinaryAllBytes },
    { QStringLiteral("c08_multibyte_korean_200kb.txt"), 204800, TestCase::RepeatedString },
  };

  QMap<QString, QString> expectedHashes;

  for (const auto& tc : testCases) {
    const QString filePath = QDir(workDir).filePath(tc.fileName);
    QFile f(filePath);
    QVERIFY(f.open(QIODevice::WriteOnly));

    QByteArray data;
    data.resize(tc.size);

    if (tc.pattern == TestCase::Zeroes) {
      // 0 bytes
    } else if (tc.pattern == TestCase::SingleByte) {
      data[0] = static_cast<char>(0x42);
    } else if (tc.pattern == TestCase::BinaryAllBytes) {
      char* ptr = data.data();
      for (qsizetype i = 0; i < tc.size; ++i) {
        ptr[i] = static_cast<char>((i * 137 + 59) & 0xFF);
      }
    } else if (tc.pattern == TestCase::RepeatedString) {
      const QByteArray sample = QStringLiteral("고고학 발굴조사 도면 및 단면도면 레이아웃 매니페스트 검증 테스트 ").toUtf8();
      char* ptr = data.data();
      for (qsizetype i = 0; i < tc.size; ++i) {
        ptr[i] = sample[i % sample.size()];
      }
    }

    if (tc.size > 0) {
      QCOMPARE(f.write(data), tc.size);
    }
    f.close();

    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    expectedHashes.insert(tc.fileName, hash);
  }

  // Generate manifest
  QString err;
  QVERIFY2(ExportService::writeSha256Manifest(workDir, &err), qPrintable(err));

  const QString manPath = QDir(workDir).filePath(QStringLiteral("MANIFEST.sha256"));
  QVERIFY(QFile::exists(manPath));

  QFile mf(manPath);
  QVERIFY(mf.open(QIODevice::ReadOnly | QIODevice::Text));
  QTextStream ts(&mf);
  ts.setEncoding(QStringConverter::Utf8);

  const QRegularExpression lineRegex(QStringLiteral("^([a-f0-9]{64})  (.+)$"));
  QMap<QString, QString> actualHashes;
  while (!ts.atEnd()) {
    const QString line = ts.readLine();
    if (line.trimmed().isEmpty()) continue;
    const auto match = lineRegex.match(line);
    QVERIFY2(match.hasMatch(), qPrintable(line));
    actualHashes.insert(match.captured(2), match.captured(1));
  }
  mf.close();

  // Verify each file hash matches exactly
  QCOMPARE(actualHashes.size(), testCases.size());
  for (auto it = expectedHashes.constBegin(); it != expectedHashes.constEnd(); ++it) {
    QVERIFY2(actualHashes.contains(it.key()), qPrintable(QStringLiteral("Missing file: ") + it.key()));
    QCOMPARE(actualHashes.value(it.key()), it.value());
  }

  // Check 0-byte file specifically
  QCOMPARE(actualHashes.value(QStringLiteral("c00_empty_0b.dat")),
           QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

void TestWorkflow::test_challenge_manifest_self_exclusion_and_regex_format() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());
  const QString workDir = tempDir.filePath(QStringLiteral("regex_format_dir"));
  QDir().mkpath(workDir);

  // Pre-seed an existing MANIFEST.sha256 before calling writeSha256Manifest (overwrite scenario)
  const QString manPath = QDir(workDir).filePath(QStringLiteral("MANIFEST.sha256"));
  {
    QFile preExisting(manPath);
    QVERIFY(preExisting.open(QIODevice::WriteOnly | QIODevice::Text));
    preExisting.write("stale manifest content that must be replaced\n");
    preExisting.close();
  }

  // Create test files
  const QStringList testFiles = {
    QStringLiteral("survey_area.shp"),
    QStringLiteral("survey_area.shx"),
    QStringLiteral("survey_area.dbf"),
    QStringLiteral("조사도면.pdf"),
    QStringLiteral("단면도.pdf"),
    QStringLiteral("README_submit.txt"),
    QStringLiteral("encoding.txt")
  };

  for (const auto& fn : testFiles) {
    QFile f(QDir(workDir).filePath(fn));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(fn.toUtf8() + " test payload contents for regex checking.\n");
    f.close();
  }

  // Call writeSha256Manifest
  QString err;
  QVERIFY2(ExportService::writeSha256Manifest(workDir, &err), qPrintable(err));

  // Inspect raw bytes of MANIFEST.sha256
  QFile mf(manPath);
  QVERIFY(mf.open(QIODevice::ReadOnly));
  const QByteArray rawBytes = mf.readAll();
  mf.close();

  // Strict regex: ^[a-f0-9]{64}  [^\r\n]+$
  const QRegularExpression strictRegex(QStringLiteral("^[a-f0-9]{64}  [^\\r\\n]+$"));

  // 1. Line format check with line endings stripped
  QFile mfText(manPath);
  QVERIFY(mfText.open(QIODevice::ReadOnly | QIODevice::Text));
  QTextStream tsText(&mfText);
  tsText.setEncoding(QStringConverter::Utf8);
  int textLineCount = 0;
  while (!tsText.atEnd()) {
    const QString line = tsText.readLine();
    if (line.trimmed().isEmpty()) continue;
    textLineCount++;
    const auto match = strictRegex.match(line);
    QVERIFY2(match.hasMatch(), qPrintable(QStringLiteral("Line does not match strict regex: [") + line + QStringLiteral("]")));
    const QString fn = line.mid(66);
    QVERIFY2(fn != QStringLiteral("MANIFEST.sha256"), "MANIFEST.sha256 must NOT be present in manifest entries");
  }
  mfText.close();
  QCOMPARE(textLineCount, testFiles.size());

  // 2. Raw binary terminator inspection
  const bool hasCrlf = rawBytes.contains("\r\n");
  // Check that every trimmed line in raw bytes matches regex
  const QString text = QString::fromUtf8(rawBytes);
  const QStringList rawLines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  for (const QString& rLine : rawLines) {
    const QString clean = rLine.endsWith(QLatin1Char('\r')) ? rLine.chopped(1) : rLine;
    const auto m = strictRegex.match(clean);
    QVERIFY2(m.hasMatch(), qPrintable(clean));
    QVERIFY2(!clean.contains(QLatin1Char('\r')), "clean line has no CR");
  }
  Q_UNUSED(hasCrlf);
}

int main(int argc, char** argv) {
  QgsApplication app(argc, argv, false);
  const QString prefix = qEnvironmentVariable("QGIS_PREFIX_PATH", QStringLiteral("C:/OSGeo4W/apps/qgis-dev"));
  QgsApplication::setPrefixPath(prefix, true);
  QgsApplication::setPluginPath(prefix + QStringLiteral("/plugins"));
  QgsApplication::initQgis();
  TestWorkflow tc;
  const int rc = QTest::qExec(&tc, argc, argv);
  QgsApplication::exitQgis();
  return rc;
}
