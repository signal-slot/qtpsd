import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 300,
      width: 400,
      child: Stack(
        children: [
          Positioned(
            height: 300,
            left: 0,
            top: 0,
            width: 400,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 300,
              width: 400,
            ),
          ),
          Positioned(
            height: 33,
            left: 81,
            top: 122,
            width: 162,
            child: Image.asset(
              "assets/images/warp_wave.png", 
              fit: BoxFit.contain,
              height: 33,
              width: 162,
            ),
          ),
        ],
      ),
    );
  }
}
