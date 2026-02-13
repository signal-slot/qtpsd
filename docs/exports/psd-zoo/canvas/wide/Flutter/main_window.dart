import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 200,
      width: 2000,
      child: Stack(
        children: [
          Positioned(
            height: 200,
            left: 0,
            top: 0,
            width: 2000,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 2000,
            ),
          ),
          Positioned(
            height: 200,
            left: 0,
            top: 0,
            width: 2000,
            child: Image.asset(
              "assets/images/content.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 2000,
            ),
          ),
        ],
      ),
    );
  }
}
