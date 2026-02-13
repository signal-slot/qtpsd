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
            height: 150,
            left: 25,
            top: 25,
            width: 150,
            child: Image.asset(
              "assets/images/partial.png", 
              fit: BoxFit.contain,
              height: 150,
              width: 150,
            ),
          ),
        ],
      ),
    );
  }
}
