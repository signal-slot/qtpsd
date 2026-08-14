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
            params: const {'uType': 3.0, 'adjWeight': 1, 'exposure': 1.5, 'gamma': 1, 'offset': 0},
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
                    "assets/images/layer_1.png", 
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
