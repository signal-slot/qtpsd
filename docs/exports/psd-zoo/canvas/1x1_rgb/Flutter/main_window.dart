import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 1,
      width: 1,
      child: Stack(
        children: [
          Positioned(
            height: 1,
            left: 0,
            top: 0,
            width: 1,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 1,
              width: 1,
            ),
          ),
          Positioned(
            height: 1,
            left: 0,
            top: 0,
            width: 1,
            child: Image.asset(
              "assets/images/pixel.png", 
              fit: BoxFit.contain,
              height: 1,
              width: 1,
            ),
          ),
        ],
      ),
    );
  }
}
