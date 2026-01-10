{
  "name": "sceneDrawVbuffer",
  "entryPoints": [
    {
      "name": "vertexMain",
      "path": "scene/draw/vbuffer.slang"
    },
    {
      "name": "pixelMain",
      "path": "scene/draw/vbuffer.slang"
    }
  ],
  "rasterizationInfo": {
    "colors": [
      {
        "format": "R32_UINT",
        "name": "vbuffer"
      }
    ],
    "depth": "D32_FLOAT",
    "alphaBlending": "None"
  },
  "bindlessSetReference": "main_materials" 
}