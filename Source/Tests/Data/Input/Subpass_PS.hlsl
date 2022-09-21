// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

[[vk::input_attachment_index(0)]]
SubpassInput subpassInput0;

[[vk::input_attachment_index(1)]] 
SubpassInput<uint> subpassInput1;

[[vk::input_attachment_index(2)]] 
SubpassInputMS<float2> subpassInput2;

void main(out float4 rt0 : SV_Target0, out uint rt1 : SV_Target1, out float2 rt2 : SV_Target2)
{
    rt0 = subpassInput0.SubpassLoad() + subpassInput1.SubpassLoad() + float4(subpassInput2.SubpassLoad(1), 0, 0);
    rt1 = 0;
    rt2 = 0;
}
