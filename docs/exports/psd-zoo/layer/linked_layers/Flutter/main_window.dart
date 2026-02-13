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
            height: 200,
            left: 0,
            top: 0,
            width: 100,
            child: Image.asset(
              "assets/images/linked_a.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 100,
            ),
          ),
          Positioned(
            height: 200,
            left: 100,
            top: 0,
            width: 100,
            child: Image.asset(
              "assets/images/linked_b.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 100,
            ),
          ),
        ],
      ),
    );
  }
}
