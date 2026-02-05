import SwiftUI

struct MainView: View {
    var body: some View {
        ZStack {
            Group {
                Rectangle
                    .fill(Color(red: 0.561, green: 0.773, blue: 0.953))
                    .frame(width: 320.0, height: 240.0)
                    .position(x: 160.0, y: 120.0)
                Path { path in
                    path.move(to: CGPoint(x: 66.5, y: 2.0))
                    path.addCurve(to: CGPoint(x: 119.0, y: 20.4), control1: CGPoint(x: 66.5, y: 2.0), control2: CGPoint(x: 119.0, y: 20.4))
                    path.addCurve(to: CGPoint(x: 132.0, y: 61.8), control1: CGPoint(x: 119.0, y: 20.4), control2: CGPoint(x: 132.0, y: 61.8))
                    path.addCurve(to: CGPoint(x: 95.7, y: 95.0), control1: CGPoint(x: 132.0, y: 61.8), control2: CGPoint(x: 95.7, y: 95.0))
                    path.addCurve(to: CGPoint(x: 37.3, y: 95.0), control1: CGPoint(x: 95.7, y: 95.0), control2: CGPoint(x: 37.3, y: 95.0))
                    path.addCurve(to: CGPoint(x: 1.0, y: 61.8), control1: CGPoint(x: 37.3, y: 95.0), control2: CGPoint(x: 1.0, y: 61.8))
                    path.addCurve(to: CGPoint(x: 14.0, y: 20.4), control1: CGPoint(x: 1.0, y: 61.8), control2: CGPoint(x: 14.0, y: 20.4))
                    path.addCurve(to: CGPoint(x: 66.5, y: 2.0), control1: CGPoint(x: 14.0, y: 20.4), control2: CGPoint(x: 66.5, y: 2.0))
                    path.closeSubpath()
                }
                    .fill(Color(red: 0.353, green: 0.435, blue: 0.055))
                    .position(x: 131.0, y: 122.0)
                    .opacity(0.37)
            }
        }
        .frame(width: 320.0, height: 240.0)
    }
}

#Preview {
    MainView()
}
