import SwiftUI

struct MainView: View {
    var body: some View {
        ZStack {
            Group {
                Rectangle
                    .fill(Color(red: 0.365, green: 0.322, blue: 0.024))
                    .frame(width: 320.0, height: 240.0)
                    .position(x: 160.0, y: 120.0)
                Group {
                    Rectangle
                        .fill(Color(red: 0.961, green: 0.961, blue: 0.929))
                        .frame(width: 160.0, height: 60.0)
                        .position(x: 129.5, y: 110.0)
                    Rectangle
                        .fill(Color(red: 0.969, green: 0.906, blue: 0.565))
                        .frame(width: 155.0, height: 32.0)
                        .position(x: 130.5, y: 97.0)
                    Text("Example1")
                        .font(.custom("SourceHanSans-Medium", size: 30))
                        .foregroundStyle(Color(red: 0.000, green: 0.000, blue: 0.000))
                        .multilineTextAlignment(.center)
                        .frame(width: 160.0, height: 33.0)
                        .position(x: 130.0, y: 96.5)
                }
            }
        }
        .frame(width: 320.0, height: 240.0)
    }
}

#Preview {
    MainView()
}
