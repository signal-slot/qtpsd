import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 200,
      width: 300,
      child: Stack(
        children: [
          Positioned(
            height: 200,
            left: 0,
            top: 0,
            width: 300,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 300,
            ),
          ),
          Positioned(
            height: 100,
            left: 75,
            top: 50,
            width: 150,
            child: Image.asset(
              "assets/images/outer_glow_layer.png", 
              fit: BoxFit.contain,
              height: 100,
              width: 150,
            ),
          ),
        ],
      ),
    );
  }
}
