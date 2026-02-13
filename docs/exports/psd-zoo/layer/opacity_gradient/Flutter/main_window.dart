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
            height: 200,
            left: 0,
            top: 0,
            width: 60,
            child: Image.asset(
              "assets/images/d002afde4b1f5710.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 60,
            ),
          ),
          Positioned(
            height: 200,
            left: 60,
            top: 0,
            width: 60,
            child: Opacity(
              opacity: 0.74902,
              child: Image.asset(
                "assets/images/8ecc52d841cb1fcf.png", 
                fit: BoxFit.contain,
                height: 200,
                width: 60,
              ),
            ),
          ),
          Positioned(
            height: 200,
            left: 120,
            top: 0,
            width: 60,
            child: Opacity(
              opacity: 0.501961,
              child: Image.asset(
                "assets/images/4bbbc83c0c2897a9.png", 
                fit: BoxFit.contain,
                height: 200,
                width: 60,
              ),
            ),
          ),
          Positioned(
            height: 200,
            left: 180,
            top: 0,
            width: 60,
            child: Opacity(
              opacity: 0.25098,
              child: Image.asset(
                "assets/images/08dd8a50bb4e0e66.png", 
                fit: BoxFit.contain,
                height: 200,
                width: 60,
              ),
            ),
          ),
          Positioned(
            height: 200,
            left: 240,
            top: 0,
            width: 60,
            child: Opacity(
              opacity: 0.101961,
              child: Image.asset(
                "assets/images/d3d4e660567819cb.png", 
                fit: BoxFit.contain,
                height: 200,
                width: 60,
              ),
            ),
          ),
        ],
      ),
    );
  }
}
