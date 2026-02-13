import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 200,
      width: 200,
      child: Stack(
        children: [
          Positioned(
            height: 200,
            left: 0,
            top: 0,
            width: 200,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 200,
            ),
          ),
          Positioned(
            height: 200,
            left: 0,
            top: 0,
            width: 200,
            child: Image.asset(
              "assets/images/this_is_a_very_long_layer_name_that_tests_the_maximum_length_handling_of_layer_names_in_the_psd_file_format_specification_which_should_support_quite_long_names_via_the_luni_additional_layer_information_block.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 200,
            ),
          ),
        ],
      ),
    );
  }
}
