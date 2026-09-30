#pragma once

class QgsMapLayer;
class QgsProject;

// One place that decides what a layer is for edit, snap, legend and delete rules.
//
// The role is written when the layer is created (LayerOps::markSurveyLayer /
// markCadastralLayer / markReferenceLayer) and saved with the project as
// ka_hgis/layer_role. The logical identity of survey data is ka_hgis/layer_key;
// Korean titles are labels only. Display names are read by legacyGuess() and
// legacyLooksLikeBackground() alone, for layers saved before roles existed.
namespace LayerRole {

enum class Kind { Unknown, Survey, Cadastral, Reference };

// Stored decision only: the ka_hgis/cadastral flag, then ka_hgis/layer_role.
Kind stored(const QgsMapLayer* layer);

// stored() -> any layer_key means survey data -> legacyGuess().
Kind resolve(const QgsMapLayer* layer);

// Name/provider guess for a layer without a stored role or key.
Kind legacyGuess(const QgsMapLayer* layer);

// Wider guess used only by LayerOps::isReferenceOrBasemapLayer for key-less,
// role-less layers: thematic names, a "참조" parent group, key-less rasters.
bool legacyLooksLikeBackground(const QgsMapLayer* layer);

// Writes the resolved role on every layer that has none, so later renames never
// change what a layer is. Keeps the project's dirty flag: opening a survey must
// not mark it as changed. Returns the number of layers written.
int persistLegacyRoles(QgsProject* project);

}  // namespace LayerRole
