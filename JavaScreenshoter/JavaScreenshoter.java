import java.awt.Rectangle;
import java.awt.Robot;
import java.io.File;
import java.awt.image.BufferedImage;
import javax.imageio.ImageIO;


public class JavaScreenshoter {

    public static void main(String[] args) throws Exception {
    	Robot robot = new Robot();
    	BufferedImage image = robot.createScreenCapture(new Rectangle(0, 0, 512, 512));
    	File file = new File("screenshot.png");
    	ImageIO.write(image, "png", file);
    }

}