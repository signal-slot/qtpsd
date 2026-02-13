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
            height: 50,
            left: 0,
            top: 0,
            width: 200,
            child: Image.asset(
              "assets/images/d953ee13be5cbb46.png", 
              fit: BoxFit.contain,
              height: 50,
              width: 200,
            ),
          ),
          Positioned(
            height: 50,
            left: 0,
            top: 50,
            width: 200,
            child: Image.asset(
              "assets/images/471b942182a4e41b.png", 
              fit: BoxFit.contain,
              height: 50,
              width: 200,
            ),
          ),
          Positioned(
            height: 50,
            left: 0,
            top: 100,
            width: 200,
            child: Image.asset(
              "assets/images/4b3d9f4f6e50544b.png", 
              fit: BoxFit.contain,
              height: 50,
              width: 200,
            ),
          ),
          Positioned(
            height: 50,
            left: 0,
            top: 150,
            width: 200,
            child: Image.asset(
              "assets/images/668aa0e7340f06f7.png", 
              fit: BoxFit.contain,
              height: 50,
              width: 200,
            ),
          ),
        ],
      ),
    );
  }
}
