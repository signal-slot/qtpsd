import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 2000,
      width: 200,
      child: Stack(
        children: [
          Positioned(
            height: 2000,
            left: 0,
            top: 0,
            width: 200,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 2000,
              width: 200,
            ),
          ),
          Positioned(
            height: 2000,
            left: 0,
            top: 0,
            width: 200,
            child: Image.asset(
              "assets/images/content.png", 
              fit: BoxFit.contain,
              height: 2000,
              width: 200,
            ),
          ),
        ],
      ),
    );
  }
}
