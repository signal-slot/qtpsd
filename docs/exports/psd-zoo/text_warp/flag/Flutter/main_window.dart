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
            height: 76,
            left: 103,
            top: 100,
            width: 85,
            child: Image.asset(
              "assets/images/warp_flag.png", 
              fit: BoxFit.contain,
              height: 76,
              width: 85,
            ),
          ),
        ],
      ),
    );
  }
}
