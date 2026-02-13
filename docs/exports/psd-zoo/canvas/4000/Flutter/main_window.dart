import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 4000,
      width: 4000,
      child: Stack(
        children: [
          Positioned(
            height: 4000,
            left: 0,
            top: 0,
            width: 4000,
            child: Image.asset(
              "assets/images/background.png", 
              fit: BoxFit.contain,
              height: 4000,
              width: 4000,
            ),
          ),
          Positioned(
            height: 3000,
            left: 500,
            top: 500,
            width: 3000,
            child: Image.asset(
              "assets/images/big_content.png", 
              fit: BoxFit.contain,
              height: 3000,
              width: 3000,
            ),
          ),
        ],
      ),
    );
  }
}
