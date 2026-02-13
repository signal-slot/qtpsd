import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 300,
      width: 300,
      child: Stack(
        children: [
          Positioned(
            height: 300,
            left: 0,
            top: 0,
            width: 300,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 300,
              width: 300,
            ),
          ),
          Positioned(
            height: 213,
            left: 44,
            top: 44,
            width: 212,
            child: Image.asset(
              "assets/images/rotated.png", 
              fit: BoxFit.contain,
              height: 213,
              width: 212,
            ),
          ),
        ],
      ),
    );
  }
}
