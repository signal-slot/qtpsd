import QtQuick

Item {
    height: 200
    width: 200
    Item {
        id: _adj_src_0
        anchors.fill: parent
        layer.enabled: true
        Image {
            fillMode: Image.PreserveAspectFit
            height: 200
            source: "images/background.png"
            width: 200
            x: 0
            y: 0
        }
        Image {
            fillMode: Image.PreserveAspectFit
            height: 200
            source: "images/colored_content.png"
            width: 200
            x: 0
            y: 0
        }
        layer.effect: ShaderEffect {
            fragmentShader: "adjustment.frag.qsb"
            property int adjustmentType: 12
            property real bal_hi_cr: 0
            property real bal_hi_mg: 0
            property real bal_hi_yb: 0
            property real bal_mid_cr: 0
            property real bal_mid_mg: 0
            property real bal_mid_yb: 0
            property real bal_preserveLum: 0
            property real bal_shadow_cr: 0
            property real bal_shadow_mg: 0
            property real bal_shadow_yb: 0
            property real brightness: 0
            property real bw_blue: 20
            property real bw_cyan: 60
            property real bw_green: 40
            property real bw_magenta: 80
            property real bw_red: 40
            property real bw_yellow: 60
            property real contrast: 0
            property real exposure: 0
            property real gamma: 1
            property real hueShift: 0
            property real lightnessShift: 0
            property real lvlB_highlightIn: 1
            property real lvlB_highlightOut: 1
            property real lvlB_midtone: 1
            property real lvlB_shadowIn: 0
            property real lvlB_shadowOut: 0
            property real lvlG_highlightIn: 1
            property real lvlG_highlightOut: 1
            property real lvlG_midtone: 1
            property real lvlG_shadowIn: 0
            property real lvlG_shadowOut: 0
            property real lvlR_highlightIn: 1
            property real lvlR_highlightOut: 1
            property real lvlR_midtone: 1
            property real lvlR_shadowIn: 0
            property real lvlR_shadowOut: 0
            property real lvl_highlightIn: 1
            property real lvl_highlightOut: 1
            property real lvl_midtone: 1
            property real lvl_shadowIn: 0
            property real lvl_shadowOut: 0
            property real mixr_mono: 0
            property real mixr_outB_b: 100
            property real mixr_outB_c: 0
            property real mixr_outB_g: 0
            property real mixr_outB_r: 0
            property real mixr_outG_b: 0
            property real mixr_outG_c: 0
            property real mixr_outG_g: 100
            property real mixr_outG_r: 0
            property real mixr_outR_b: 0
            property real mixr_outR_c: 0
            property real mixr_outR_g: 0
            property real mixr_outR_r: 100
            property real offset: 0
            property real phfl_b: 1
            property real phfl_density: 0
            property real phfl_g: 1
            property real phfl_preserveLum: 0
            property real phfl_r: 1
            property real posterizeLevels: 4
            property real saturationShift: 0
            property real thresholdLevel: 0.5
            property real vibrance: 0
            property real vibranceSat: 0
        }
    }
}
