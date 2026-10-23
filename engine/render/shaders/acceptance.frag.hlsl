struct PixelInput { float4 position : SV_Position; };
float4 main(PixelInput input) : SV_Target {
    return float4(0.25, 0.5, 0.75, 1.0);
}
