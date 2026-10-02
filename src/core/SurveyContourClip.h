#pragma once

class OGRLayer;
class QString;

// Cuts every feature to the polygon WKT. Pieces outside the polygon are removed.
// Returns the number of features kept. An empty WKT leaves the layer unchanged.
// applied reports whether every feature was really cut: false when the WKT cannot be read
// (the layer is left whole) or when a feature could not be intersected (that one is kept whole).
int clipSurveyLayerToWkt(OGRLayer* layer, const QString& wkt, bool* applied = nullptr);
