{
  "name": "sceneDrawForwardPbr",
  "entryPoints": [
    {
      "name": "vertexMain",
      "path": "scene/draw/forwardPbr.slang"
    },
    {
      "name": "pixelMain",
      "path": "scene/draw/forwardPbr.slang"
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
    "depth": "D32_FLOAT"
  },
  "bindlessSetReference": "main_materials" 
}