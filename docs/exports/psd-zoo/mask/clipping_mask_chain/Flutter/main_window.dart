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
          Image.asset(
            "assets/images/", 
            fit: BoxFit.contain,
            height: 0,
            width: 0,
          ),
          Positioned(
            height: 140,
            left: 30,
            top: 30,
            width: 140,
            child: Image.asset(
              "assets/images/base.png", 
              fit: BoxFit.contain,
              height: 140,
              width: 140,
            ),
          ),
          Positioned(
            height: 200,
            left: 0,
            top: 0,
            width: 200,
            child: Image.asset(
              "assets/images/clip_1.png", 
              fit: BoxFit.contain,
              height: 200,
              width: 200,
            ),
          ),
          Positioned(
            height: 100,
            left: 0,
            top: 0,
            width: 200,
            child: Opacity(
              opacity: 0.501961,
              child: Image.asset(
                "assets/images/clip_2.png", 
                fit: BoxFit.contain,
                height: 100,
                width: 200,
              ),
            ),
          ),
        ],
      ),
    );
  }
}
