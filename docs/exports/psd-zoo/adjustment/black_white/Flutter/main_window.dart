import './psd_adjustment.dart';
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
          PsdAdjustment(
            params: const {'uType': 12.0, 'adjWeight': 1, 'bw_blue': 20, 'bw_cyan': 60, 'bw_green': 40, 'bw_magenta': 80, 'bw_red': 40, 'bw_yellow': 60},
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
                    "assets/images/colored_content.png", 
                    fit: BoxFit.contain,
                    height: 200,
                    width: 200,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
