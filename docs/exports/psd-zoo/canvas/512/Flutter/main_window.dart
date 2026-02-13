import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 512,
      width: 512,
      child: Stack(
        children: [
          Positioned(
            height: 512,
            left: 0,
            top: 0,
            width: 512,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 512,
              width: 512,
            ),
          ),
          Positioned(
            height: 512,
            left: 0,
            top: 0,
            width: 512,
            child: Image.asset(
              "assets/images/content.png", 
              fit: BoxFit.contain,
              height: 512,
              width: 512,
            ),
          ),
        ],
      ),
    );
  }
}
