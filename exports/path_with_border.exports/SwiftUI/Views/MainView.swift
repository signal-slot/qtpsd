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
                    path.move(to: CGPoint(x: 76.7, y: 11.0))
                    path.addCurve(to: CGPoint(x: 129.2, y: 29.7), control1: CGPoint(x: 76.7, y: 11.0), control2: CGPoint(x: 129.2, y: 29.7))
                    path.addCurve(to: CGPoint(x: 141.9, y: 71.1), control1: CGPoint(x: 129.2, y: 29.7), control2: CGPoint(x: 141.9, y: 71.1))
                    path.addCurve(to: CGPoint(x: 105.4, y: 104.1), control1: CGPoint(x: 141.9, y: 71.1), control2: CGPoint(x: 105.4, y: 104.1))
                    path.addCurve(to: CGPoint(x: 47.1, y: 103.9), control1: CGPoint(x: 105.4, y: 104.1), control2: CGPoint(x: 47.1, y: 103.9))
                    path.addCurve(to: CGPoint(x: 10.9, y: 70.5), control1: CGPoint(x: 47.1, y: 103.9), control2: CGPoint(x: 10.9, y: 70.5))
                    path.addCurve(to: CGPoint(x: 24.1, y: 29.2), control1: CGPoint(x: 10.9, y: 70.5), control2: CGPoint(x: 24.1, y: 29.2))
                    path.addCurve(to: CGPoint(x: 76.7, y: 11.0), control1: CGPoint(x: 24.1, y: 29.2), control2: CGPoint(x: 76.7, y: 11.0))
                    path.closeSubpath()
                }
                    .fill(Color(red: 0.353, green: 0.435, blue: 0.055))
                    .stroke(Color(red: 0.898, green: 0.988, blue: 0.078), lineWidth: 17.0)
                    .position(x: 130.5, y: 122.0)
                    .opacity(0.37)
                Path { path in
                    path.move(to: CGPoint(x: 66.5, y: 1.0))
                    path.addCurve(to: CGPoint(x: 119.0, y: 19.4), control1: CGPoint(x: 66.5, y: 1.0), control2: CGPoint(x: 119.0, y: 19.4))
                    path.addCurve(to: CGPoint(x: 132.0, y: 60.8), control1: CGPoint(x: 119.0, y: 19.4), control2: CGPoint(x: 132.0, y: 60.8))
                    path.addCurve(to: CGPoint(x: 95.7, y: 94.0), control1: CGPoint(x: 132.0, y: 60.8), control2: CGPoint(x: 95.7, y: 94.0))
                    path.addCurve(to: CGPoint(x: 37.3, y: 94.0), control1: CGPoint(x: 95.7, y: 94.0), control2: CGPoint(x: 37.3, y: 94.0))
                    path.addCurve(to: CGPoint(x: 1.0, y: 60.8), control1: CGPoint(x: 37.3, y: 94.0), control2: CGPoint(x: 1.0, y: 60.8))
                    path.addCurve(to: CGPoint(x: 14.0, y: 19.4), control1: CGPoint(x: 1.0, y: 60.8), control2: CGPoint(x: 14.0, y: 19.4))
                    path.addCurve(to: CGPoint(x: 66.5, y: 1.0), control1: CGPoint(x: 14.0, y: 19.4), control2: CGPoint(x: 66.5, y: 1.0))
                    path.closeSubpath()
                }
                    .fill(Color(red: 0.353, green: 0.435, blue: 0.055))
                    .position(x: 236.5, y: 122.5)
                    .opacity(0.37)
            }
        }
        .frame(width: 320.0, height: 240.0)
    }
}

#Preview {
    MainView()
}
