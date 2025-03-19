using System;
using System.Collections;
using System.Collections.Generic;
using DG.Tweening;
using UnityEngine;

public class Coursor : MonoBehaviour
{
    enum HandState {Scaning,Holding }
    HandState handState = HandState.Scaning;
    public float Distance;
    
    private Transform holdingTranform;
    // Start is called before the first frame update
    void Start()
    {
        
    }

    // Update is called once per frame
    void Update()
    {
        
        if (handState == HandState.Scaning)
        {
            
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
            if (!Input.GetMouseButton(0))
            {
                handState = HandState.Scaning;
                return;
            }
            Vector3 newpos = Camera.main.ScreenToWorldPoint(Input.mousePosition);
            newpos.z = 0;
            holdingTranform.position = newpos;
        }
    }
}
