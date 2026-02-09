import SwiftUI

struct MainView: View {
    var body: some View {
        ZStack {
            Group {
                Rectangle
                    .fill(Color(red: 0.969, green: 0.906, blue: 0.565))
                    .frame(width: 320.0, height: 240.0)
                    .position(x: 160.0, y: 120.0)
                Group {
                    RoundedRectangle(cornerRadius: 20.0)
                        .fill(Color(red: 0.831, green: 0.961, blue: 0.788))
                        .frame(width: 180.0, height: 50.0)
                        .position(x: 130.0, y: 75.0)
                    Group {
                        Rectangle
                            .fill(Color(red: 0.129, green: 0.475, blue: 0.012))
                            .frame(width: 137.0, height: 10.0)
                            .position(x: 141.5, y: 95.5)
                        Rectangle
                            .fill(Color(red: 0.369, green: 0.533, blue: 0.906))
                            .frame(width: 23.0, height: 12.0)
                            .position(x: 91.5, y: 88.0)
                    }
                    Text("Example1")
                        .font(.custom("Source Han Sans", size: 30))
                        .foregroundStyle(Color(red: 0.000, green: 0.000, blue: 0.000))
                        .multilineTextAlignment(.center)
                        .frame(width: 160.0, height: 33.0)
                        .position(x: 130.0, y: 79.5)
                }
                Group {
                    RoundedRectangle(cornerRadius: 20.0)
                        .fill(Color(red: 0.831, green: 0.961, blue: 0.788))
                        .frame(width: 180.0, height: 50.0)
                        .position(x: 130.0, y: 160.0)
                    Group {
                        Rectangle
                            .fill(Color(red: 0.129, green: 0.475, blue: 0.012))
                            .frame(width: 137.0, height: 10.0)
                            .position(x: 141.5, y: 180.0)
                        Rectangle
                            .fill(Color(red: 0.369, green: 0.533, blue: 0.906))
                            .frame(width: 23.0, height: 12.0)
                            .position(x: 191.5, y: 173.0)
                    }
                    Text("Example1")
                        .font(.custom("Source Han Sans", size: 30))
                        .foregroundStyle(Color(red: 0.000, green: 0.000, blue: 0.000))
                        .multilineTextAlignment(.center)
                        .frame(width: 160.0, height: 33.0)
                        .position(x: 130.0, y: 159.5)
                }
            }
        }
        .frame(width: 320.0, height: 240.0)
    }
}

#Preview {
    MainView()
}
