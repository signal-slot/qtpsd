import QtQuick

Item {
    height: 200
    width: 200
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
        source: "images/this_is_a_very_long_layer_name_that_tests_the_maximum_length_handling_of_layer_names_in_the_psd_file_format_specification_which_should_support_quite_long_names_via_the_luni_additional_layer_information_block.png"
        width: 200
        x: 0
        y: 0
    }
}
