{
  "name": "sceneDrawMomentBasedOitForwardPbr",
  "entryPoints": [
    {
      "name": "vertexMain",
      "path": "scene/oit/moment/forwardPbr.slang"
    },
    {
      "name": "pixelMain",
      "path": "scene/oit/moment/forwardPbr.slang"
    }
  ],
  "variants": [
    {
      "name": "tiled",
      "defines": {"USE_TILED_LIGHTING": "1"}
    },
    {
      "name": "clustered",
      "defines": {"USE_CLUSTERED_LIGHTING": "1"}
    },
    {
      "name": "hybrid",
      "defines": {"USE_HYBRID_LIGHTING": "1"}
    }
  ],
  "rasterizationInfo": {
    "colors": [
      {
        "format": "RGBA16_FLOAT",
        "name": "color"
      }
    ],
    "depth": "D32_FLOAT",
    "depthMode": "Read",
    "alphaBlending": "Additive"
  }
}