using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.SceneManagement;
public class SceneManager : MonoBehaviour
{
    public static SceneManager instance;
    // Start is called before the first frame update
    void Start()
    {
        instance = this;
    }

    public static void loadMainScene()
    {
        UnityEngine.SceneManagement.SceneManager.LoadScene(0);
    }
    public static void loadGameOverScene()
    {
        UnityEngine.SceneManagement.SceneManager.LoadScene(1);
    }
    
    public static void loadWinScene()
    {
        UnityEngine.SceneManagement.SceneManager.LoadScene(2);
    }
}
