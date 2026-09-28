#pragma once

class OGRLayer;
class QString;

// Cuts every feature to the polygon WKT. Pieces outside the polygon are removed.
// Returns the number of features kept. An empty WKT leaves the layer unchanged.
int clipSurveyLayerToWkt(OGRLayer* layer, const QString& wkt);
