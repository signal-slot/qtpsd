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
            width: 300,
            child: Container(
              decoration: BoxDecoration(
                gradient: LinearGradient(
                  begin: Alignment(1, 0),
                  colors: [
                    Color.fromARGB(255, 255, 0, 0),
                    Color.fromARGB(255, 0, 255, 0),
                    Color.fromARGB(255, 0, 0, 255),
                  ],
                  end: Alignment(-1, 0),
                  stops: [
                    0,
                    0.5,
                    1,
                  ],
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
