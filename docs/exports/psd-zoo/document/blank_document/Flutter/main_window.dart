import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 100,
      width: 100,
      child: Stack(
        children: [
          Image.asset(
            "assets/images/merged.png", 
            fit: BoxFit.contain,
            height: 100,
            width: 100,
          ),
        ],
      ),
    );
  }
}
