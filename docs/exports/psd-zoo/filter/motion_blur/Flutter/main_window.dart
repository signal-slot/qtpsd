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
            height: 76,
            left: 63,
            top: 62,
            width: 74,
            child: Image.asset(
              "assets/images/motion_blurred.png", 
              fit: BoxFit.contain,
              height: 76,
              width: 74,
            ),
          ),
        ],
      ),
    );
  }
}
