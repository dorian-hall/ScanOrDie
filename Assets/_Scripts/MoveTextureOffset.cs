using UnityEngine;

public class MoveTextureOffset : MonoBehaviour
{
    Renderer rend;
    [SerializeField]
    float scrollSpeed = 0.5f;

    void Start()
    {
        rend = GetComponent<Renderer>();
    }

    void Update()
    {
        float offset = Time.time * scrollSpeed;
        rend.material.mainTextureOffset = new Vector2(offset, 0);
    }
}
