{
  "name": "sceneVbufferPbr",
  "entryPoints": [
    {
      "name": "fullscreenMain",
      "path": "utility/fullscreen.slang"
    },
    {
      "name": "main",
      "path": "scene/vbuffer/vbufferPbr.slang"
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
    ]
  }
}