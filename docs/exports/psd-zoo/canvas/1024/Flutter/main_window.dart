import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 1024,
      width: 1024,
      child: Stack(
        children: [
          Positioned(
            height: 1024,
            left: 0,
            top: 0,
            width: 1024,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 1024,
              width: 1024,
            ),
          ),
          Positioned(
            height: 1024,
            left: 0,
            top: 0,
            width: 1024,
            child: Image.asset(
              "assets/images/content.png", 
              fit: BoxFit.contain,
              height: 1024,
              width: 1024,
            ),
          ),
        ],
      ),
    );
  }
}
