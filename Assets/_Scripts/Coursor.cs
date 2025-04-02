using System;
using System.Collections;
using System.Collections.Generic;
using DG.Tweening;
using UnityEngine;

public class Coursor : MonoBehaviour
{
    enum HandState {Scaning,Holding }
    [SerializeField] HandState handState = HandState.Scaning;
    public float Distance;
    public static Coursor Instance;
    public List<Transform> bombs;
    public Transform holdingTranform;

    private void Awake()
    {
        Instance = this;
    }

    // Start is called before the first frame update
    void Start()
    {
        
    }

    // Update is called once per frame
    private void FixedUpdate()
    {


        if (handState == HandState.Scaning)
        {
            float distance = float.MaxValue;
            foreach (var bomb in bombs)
            {
                float newdistance = Vector2.Distance(Camera.main.WorldToViewportPoint(bomb.position),
                    Camera.main.ScreenToViewportPoint(Input.mousePosition));
                if (newdistance < distance) Distance = newdistance;
            }
        }
        else Distance = 1;
            
        AudioManager.instance.ScanerDistence.SetValue(AudioManager.instance.gameObject,Distance);
    }

    void Update()
    {

        
        if (handState == HandState.Scaning)
        {
  
            Debug.Log(AudioManager.instance.ScanerDistence.GetValue(AudioManager.instance.gameObject));
            if (!Input.GetMouseButton(0)) return;
            RaycastHit hitInfo;
            
          
            if (Physics.Raycast(Camera.main.ScreenPointToRay(Input.mousePosition), out hitInfo))
            {
                holdingTranform = hitInfo.transform;
                handState = HandState.Holding;
            } 
        }
        else
        {
    

            if (!Input.GetMouseButton(0)|| holdingTranform == null)
            {
                handState = HandState.Scaning;
                return;
            }

            Distance = 0;
            Vector3 newpos = Camera.main.ScreenToWorldPoint(Input.mousePosition);
            newpos.z = 0;
        
            holdingTranform.position = newpos;
        }
        
    }
}
