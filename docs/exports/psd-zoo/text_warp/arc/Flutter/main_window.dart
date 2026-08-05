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
            height: 62,
            left: 56,
            top: 87,
            width: 226,
            child: Image.asset(
              "assets/images/warp_arc.png", 
              fit: BoxFit.contain,
              height: 62,
              width: 226,
            ),
          ),
        ],
      ),
    );
  }
}
