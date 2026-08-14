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
            params: const {'uType': 5.0, 'adjWeight': 1, 'bal_hi_cr': 0, 'bal_hi_mg': 0, 'bal_hi_yb': 0, 'bal_mid_cr': 0, 'bal_mid_mg': 0, 'bal_mid_yb': 0, 'bal_preserveLum': 1, 'bal_shadow_cr': 0, 'bal_shadow_mg': 0, 'bal_shadow_yb': 0},
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
                    "assets/images/base.png", 
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
