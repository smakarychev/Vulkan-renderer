module gaussianBlur;

public enum GaussianBlurDirection {Vertical, Horizontal};


public interface IGaussianBlur<ValueT> where ValueT : IArithmetic {
    public ValueT readImage(float2 uv, float scale);
    public void storeImage(uint2 coord, ValueT value);
    public float2 imageSize();
}

namespace gaussianBlur {
    public void blur5<let BlurDirection : GaussianBlurDirection, BlurT, ValueT>(BlurT blur, uint2 coord) 
        where ValueT : IArithmetic where BlurT : IGaussianBlur<ValueT> {
        ValueT blurred = ValueT(0);

        const float2 imageSizeInverse = 1.0f / blur.imageSize();
        const float2 uv = float2(coord + 0.5f) * imageSizeInverse;

        float2 offset;
        if (BlurDirection == GaussianBlurDirection.Vertical) 
            offset = float2(0.0f, 1.3333333333333333f);
        else 
            offset = float2(1.3333333333333333f, 0.0f);

        blurred = blurred.add(blur.readImage(uv, 0.29411764705882354f));
        blurred = blurred.add(blur.readImage(uv + offset * imageSizeInverse, 0.35294117647058826f));
        blurred = blurred.add(blur.readImage(uv - offset * imageSizeInverse, 0.35294117647058826f));

        blur.storeImage(coord, blurred);
    }
}

