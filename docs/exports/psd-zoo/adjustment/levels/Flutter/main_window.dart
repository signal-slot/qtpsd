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
            params: const {'uType': 1.0, 'adjWeight': 1, 'lvlB_highlightIn': 1, 'lvlB_highlightOut': 1, 'lvlB_midtone': 1, 'lvlB_shadowIn': 0, 'lvlB_shadowOut': 0, 'lvlG_highlightIn': 1, 'lvlG_highlightOut': 1, 'lvlG_midtone': 1, 'lvlG_shadowIn': 0, 'lvlG_shadowOut': 0, 'lvlR_highlightIn': 1, 'lvlR_highlightOut': 1, 'lvlR_midtone': 1, 'lvlR_shadowIn': 0, 'lvlR_shadowOut': 0, 'lvl_highlightIn': 1, 'lvl_highlightOut': 1, 'lvl_midtone': 1, 'lvl_shadowIn': 0, 'lvl_shadowOut': 0},
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
