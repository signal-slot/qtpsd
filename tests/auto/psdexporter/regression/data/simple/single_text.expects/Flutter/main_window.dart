import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 240,
      width: 320,
      child: Stack(
        children: [
          Container(
            color: Color.fromARGB(255, 247, 231, 144),
            height: 240,
            width: 320,
            child: Stack(
              children: [
                Positioned(
                  height: 58,
                  left: 50,
                  top: 80,
                  width: 160,
                  child: Column(
                    children: [
                      Text(
                        "Example1", 
                        style: TextStyle(
                          color: Color.fromARGB(255, 0, 0, 0),
                          fontFamily: "SourceHanSans-Medium",
                          fontSize: 20,
                          fontVariations: [FontVariation.weight(600)],
                          height: 1,
                        ),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
