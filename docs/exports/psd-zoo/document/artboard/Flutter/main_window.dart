import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 400,
      width: 400,
      child: Stack(
        children: [
          Container(
            color: Color.fromARGB(255, 255, 255, 255),
            height: 400,
            width: 400,
            child: Stack(
              children: [
                Positioned(
                  height: 400,
                  left: 0,
                  top: 0,
                  width: 400,
                  child: Container(
                    decoration: (
                      color: Color.fromARGB(255, 255, 255, 255),
                    ),
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
